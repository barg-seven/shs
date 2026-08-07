#include <iostream>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <mutex>
#include <string>
#include <modbus/modbus.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstring>
#include "t_shs.h"
#include "t_log.h"
#include "t_config.h"
#include "t_thread_queue.h"
#include "t_status_queue.h"
#include "t_waveshare_card.h"
#include "t_web_socket.h"

std::mutex mbus_lock;
std::mutex http_response_lock;
t_thread_queue queue;
t_status_queue status_queue;
std::mutex web_clients_lock;

auto ws = t_web_socket();
auto shs_config = t_config("/etc/shs.conf");

void thread_tcp_server(t_log& msg);
void handle_http_request(const int& client,t_log& msg,const t_config& config);
void thread_modbus_h_inputs(t_log& msg,modbus_t* mb);
void thread_mbus_h_outputs(t_log& msg,modbus_t* mb);
std::string get_http_response(const std::string& content);


/**
 * @brief Nimmt TCP-Verbindungen an.
 *
 * @details
 * Diese Threadfunktion TCP-Verbindungen vom Reverse-Proxy an, und macht f. jede Verbindung
 * einen Thread auf. Die Funktion handle_http_request() verarbeitet die HTTP-Requests.
 *
 * @param msg Logging
 *
 * @author Andreas Jentsch
 * @date 23.06.2026
 */
void thread_tcp_server(t_log& msg) {

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    if (const int rc = pthread_setname_np(pthread_self(), "tcp-server"); rc != 0) {
        msg.log("ERROR","Die Funktion pthread_setname_np() konnte keinen Namen vergeben.");
        return;
    }

    constexpr int port = 8080;

    // server-Socket erstellen
    const int ss = socket(AF_INET, SOCK_STREAM, 0);
    if (ss < 0) {
        msg.log("ERROR","TCP-Server: " + std::string(std::strerror(errno)) + " Code " + std::to_string(errno));
        return;
    }

    // port-Wiederverwendung aktivieren (verhindert "Address already in use"-Fehler beim Neustart)
    constexpr int opt = 1;
    setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // server-Adresse konfigurieren
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Auf allen Schnittstellen lauschen
    address.sin_port = htons(port);

    // socket an den Port binden
    if (bind(ss, reinterpret_cast<struct sockaddr *>(&address), sizeof(address)) < 0) {

        msg.log("ERROR","TCP-Server: " + std::string(std::strerror(errno)) + " Code " + std::to_string(errno));
        return;
    }

    // Warteschlange aktivieren
    if (listen(ss, 10) < 0) {

        msg.log("ERROR","TCP-Server: " + std::string(std::strerror(errno)) + " Code " + std::to_string(errno));
        return;
    }

    msg.log("INFO","TCP-Server: Lauscht auf Port " + std::to_string(port) + ".");

    try {

        while (true) {

            sockaddr_in client_address{};
            socklen_t client_len = sizeof(client_address);

            // blockiert, bis ein neuer Client kommt
            if (int client_fd = accept(ss, reinterpret_cast<struct sockaddr *>(&client_address), &client_len); client_fd >= 0) {

                // die IP-Adresse in einen lesbaren String umwandeln
                std::string client_ip = inet_ntoa(client_address.sin_addr);

                // ist immer 127.0.0.1 (wegen Reverse Proxy)
                //msg.log("INFO","TCP-Server: Neue Verbindung von " + client_ip + ".");

                ws.ct.emplace_back(handle_http_request,client_fd,std::ref(msg),std::ref(shs_config));
            }
        }
    }
    catch (std::exception& e) {
        msg.log("ERROR","TCP-Server: " + std::string(e.what()));
    }
    catch (...) {
        std::cerr << "Unknown exception." << std::endl;
    }
}
// ----------------------------------------------------------------------------
void handle_http_request(const int& client,t_log& msg,const t_config& config) {

    char buffer[1024] = {};
    //int flag = 1;
    //setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    // den HTTP-Request vom Browser einlesen
    if (const ssize_t bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0); bytes_received > 0) {

        std::istringstream request(std::string(buffer,bytes_received)); // der HTTP-Request im Stream
        std::string http_response; // der Antwort-Header
        std::ostringstream json_response; // die Nutzdaten

        if (t_web_socket::web_socket_request(request)) {

            const std::string client_key = t_web_socket::get_client_key(request);
            const std::string accept_key = ws.create_accept_key(client_key);

            // eine Handshake-Antwort an den Browser senden
            t_web_socket::create_http_response_header(http_response, accept_key);
            send(client, http_response.c_str(), http_response.length(), 0);

            // den Client f. Push-Benachrichtigungen speichern
            ws.add_client(client);

            msg.log("INFO","Client " + std::to_string(client) + " hat eine Web-Socket-Verbindung hergestellt.");

            // die ganze Status-Queue an den Client senden
            status_queue.get_all_states_as_json(json_response);
            if (const int rc = t_web_socket::send_frame(client,json_response.str()); rc < 0) {
                msg.log("ERROR","Senden der Status-Queue fehlgeschlagen.");
            }

            // warten bis der Browser die Verbindung trennt
            bool connected = true;

            while (connected) {

                // blockiert und wartet, bis der Browser einen Befehl schickt
                std::string json = t_web_socket::receive_frame(client, connected);

                if (!connected) {
                    break; // browser hat geschlossen oder Netzwerkfehler
                }

                if (!json.empty()) {

                    msg.log("INFO","Client " + std::to_string(client) + " sendet Befehl: " + json);

                    std::map<std::string,std::string> result = t_shs::get_command(json);

                    t_command cmd = {};

                    cmd.action = std::stoi(result.at("action"));
                    cmd.card_index = config.waveshare.card.at(std::stoi(result.at("card"))).index;
                    cmd.card_address = config.waveshare.card.at(std::stoi(result.at("card"))).address;
                    cmd.output_index = std::stoi(result.at("relay"));
                    cmd.output_address = std::stoi(result.at("relay"));
                    cmd.status = std::stoi(result.at("state"));

                    switch (cmd.action) {
                        case 0: {

                            // f. das Schreiben in die Status-Queue
                            cmd.dim = false;
                            cmd.impuls = true;

                            cmd.status = 1; // den Status ueberschreiben
                            queue.push(cmd);

                            // f. den Impuls
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));

                            cmd.impuls = false; // f. das Schreiben in die Status-Queue

                            cmd.status = 0; // den Status ueberschreiben
                            queue.push(cmd);
                            break;
                        }
                        case 1: {

                            cmd.dim = true;
                            cmd.status = 1; // den Status ueberschreiben

                            queue.push(cmd);
                            break;
                        }
                        case 2: {

                            cmd.dim = true;
                            cmd.status = 0; // den Status ueberschreiben

                            queue.push(cmd);
                            break;
                        }
                        case 3: {

                            // f. das Schreiben in die Status-Queue
                            cmd.dim = false;
                            cmd.impuls = false;
                            cmd.no_impuls = true;

                            queue.push(cmd);
                            break;
                        }
                        default: {}
                    }

                    // dem Thread mbus-h-outputs Zeit geben um die Status-Queue zu aktualisieren
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    status_queue.get_all_states_as_json(json_response);
                    ws.broadcast_relays_status(json_response.str());
                }
            }

            msg.log("INFO","Client " + std::to_string(client) + " hat die Web-Socket-Verbindung getrennt.");

            // wenn die Schleife beendet ist
            ws.remove_client(client);
            close(client);
        }
        else {
            // ab hier ist es ein HTTP-Request f. eine Datei oder ein Befehl zum Schalten
            close(client);
        }
    }
}
// ----------------------------------------------------------------------------
/**
 * @brief Fragt staendig von allen Karten alle Eingaenge ab.
 *
 * @details
 * Diese Thread-Funktion fragt von allen Waveshare Relais-Karten alle digitalen Eingaenge ab.
 * Durch das Staendige anfragen, registriert diese Funktion, wenn ein Taster irgendwo im Haus
 * gedrueckt wurde.
 *
 * @param msg Wird f. das Schreiben von Logs benoetigt.
 * @param mb Modbus RTU RS485 Zeiger.
 *
 * @author Andreas Jentsch
 * @date 06.06.2026
 */
void thread_modbus_h_inputs(t_log& msg,modbus_t* mb) {

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    if (const int rc = pthread_setname_np(pthread_self(), "mbus-h-inputs"); rc != 0) {
        msg.log("ERROR","Die Funktion pthread_setname_np() konnte den Namen \"mbus-h-inputs\" nicht vergeben.");
        return;
    }

    try {

        int rc; // return code
        t_command data{};
        std::ostringstream json_response; // die Nutzdaten

        while(true) {

            // alle Relais-Karten durchlaufen und pruefen, ob an einem Eingang ein Signal anliegt
            for (auto & card : status_queue.cards) {

                // nimmt die 8 Zustaende von Eingaengen einer Relais Karte auf
                uint8_t inputs[8] = {0};

                {
                    std::lock_guard<std::mutex> lock(mbus_lock);
                    modbus_set_slave(mb, card.address); // die Modbus-Adresse der Relais Karte festlegen
                    rc = modbus_read_input_bits(mb, 0, 8, inputs);
                }

                if (rc == -1) {
                    msg.log("ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                    //throw std::runtime_error(modbus_strerror(errno));
                }

                // nach dem Lesen 15 Millisekunden warten
                std::this_thread::sleep_for(std::chrono::milliseconds(15));

                // zustaende aller 8 Digitalen Eingaenge der aktuellen Karte ermitteln
                for (int i = 0; i < 8; ++i) {

                    uint8_t status;

                    // wenn der Eingang den Status 1 hat (ein Taster wurde gedrueckt)
                    if (inputs[i] == 1 && card.inputs[i].type == "finder") {

                        // einen Eintrag in der Logdatei erzeugen
                        msg.log("INFO","Taster: Signal liegt an. Kartenadresse " + std::to_string(card.address) + " Eingang Index" + std::to_string(i));

                        {
                            // den aktuellen Zustand des Relais (Outputs) auf der Relais-Karte ermitteln
                            std::lock_guard<std::mutex> lock(mbus_lock);
                            rc = modbus_read_bits(mb,i,1,&status);
                        }

                        if (rc == -1) {
                            msg.log("ERROR","Modbus RTU RS485: Funktion modbus_read_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                        }

                        // nach dem Lesen 15 Millisekunden warten
                        std::this_thread::sleep_for(std::chrono::milliseconds(15));

                        status = (status == 0) ? 1 : 0; // invertieren (aus 0 wird 1 und aus 1 wird 0)

                        // befehl erzeugen und relais schalten
                        data.card_address = card.address;
                        data.output_index = i;
                        data.output_address = i;
                        data.status = status;
                        queue.push(data);

                        // solange warten bis der Taster losgelassen wurde
                        do {

                            // warten, dann pruefen ob der Taster losgelassen wurde
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            {
                                std::lock_guard<std::mutex> lock(mbus_lock);
                                rc = modbus_read_input_bits(mb, i, 1, &status);
                            }
                            if (rc == -1) {
                                msg.log("ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                            }
                        } while (status == 1);

                        msg.log("INFO","Taster: Signal weg. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                        // dem Thread mbus-h-outputs Zeit geben um die Status-Queue zu aktualisieren
                        /*std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        status_queue.get_all_states_as_json(json_response);
                        ws.broadcast_relays_status(json_response.str());*/
                    }


                    if (inputs[i] == 1 && card.inputs[i].type == "eltako") {

                        msg.log("INFO","Taster: Signal liegt an. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                        // befehl erzeugen
                        data.card_address = card.address;
                        data.output_index = i;
                        data.output_address = i;

                        // warten dann pruefen ob der Taster losgelassen wurde
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        {
                            std::lock_guard<std::mutex> lock(mbus_lock);
                            rc = modbus_read_input_bits(mb, i, 1, &status);
                        }
                        if (rc == -1) {
                            msg.log("ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Kartenadresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                        }

                        // der Taster wurde losgelassen, den Eltako mit Impuls schalten
                        if (status == 0) {

                            data.status = 1; // nur den Status anpassen
                            queue.push(data);

                            // f. den Impuls
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));

                            data.status = 0; // nur den Status anpassen
                            queue.push(data);

                            msg.log("INFO","Taster: Schaltimpuls gesendet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));

                            // dem Thread mbus-h-outputs Zeit geben um die Status-Queue zu aktualisieren
                            /*std::this_thread::sleep_for(std::chrono::milliseconds(50));

                            status_queue.get_all_states_as_json(json_response);
                            ws.broadcast_relays_status(json_response.str());*/
                        }
                        else { // den Dim-Vorgang starten

                            msg.log("INFO","Taster: Dim-Vorgang gestartet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));

                            // den Dimm-Vorgang starten
                            data.status = 1; // nur den Status anpassen
                            queue.push(data);

                            // solange warten bis der Taster losgelassen wurde
                            do {

                                // ist der Status 0, wurde der Taster losgelassen
                                {
                                    std::lock_guard<std::mutex> lock(mbus_lock);
                                    rc = modbus_read_input_bits(mb, i, 1, &status);
                                }
                                if (rc == -1) {
                                    msg.log("ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Kartenadresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                                }
                            } while (status == 1);

                            msg.log("INFO","Taster: Dim-Vorgang beendet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));
                            msg.log("INFO","Taster: Signal weg. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                            // das Relais abschalten
                            data.status = 0; // nur den Status anpassen
                            queue.push(data);
                        }
                    }
                }
            }
        }
    }
    catch (std::exception& e) {
        msg.log("ERROR","Exception: " + std::string(e.what()));
    }
    catch (...) {
        msg.log("ERROR","Exception: Unbekannter Fehler.");
    }

    modbus_close(mb);
    modbus_free(mb);
}
// ----------------------------------------------------------------------------
/**
 * @brief Schreibt auf Modbus RTU RS485
 * @details
 * Diese Threadfunktion schaltet die Relais auf den Relais-Karten. Ob es sich um einen
 * einfachen Schaltvorgang oder um einen Schaltimpuls handelt, wird von den andren
 * Threads per Thread-Queue gesteuert.
 *
 * @param msg Wird zum Schreiben von Logeintraegen benoetigt.
 * @param mb Modbus RTU RS485 Zeiger.
 *
 * @author Andreas Jentsch
 * @date 18.06.2026
 */
void thread_mbus_h_outputs(t_log& msg,modbus_t* mb) {

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    if (const int rc = pthread_setname_np(pthread_self(), "mbus-h-outputs"); rc != 0) {
        msg.log("ERROR","Die Funktion pthread_setname_np() konnte keinen Namen vergeben.");
        return;
    }

    try {

        int rc;
        t_command data = {};

        while (true) {

            // warten bis ein Thread push() aufruft
            queue.pop(data);

            // ein Relais schalten
            {
                std::lock_guard<std::mutex> lock(mbus_lock);

                // die Modbus-Adresse der Relais Karte festlegen
                modbus_set_slave(mb, data.card_address);

                // den neuen Status auf die Karte schreiben
                rc = modbus_write_bit(mb, data.output_address, data.status);
            }

            if (rc == -1) {
                msg.log("ERROR","Modbus RTU RS485: Funktion modbus_write_bit() Adresse " + std::to_string(1) + " Code " + std::string(modbus_strerror(errno)));
            }

            if (!data.dim) {

                // den neuen Status in die Status-Queue schreiben
                if (data.impuls) {
                    //const int s = static_cast<int>(status_queue.get_state(data.card_index,data.output_index));
                    const int s = status_queue.get_state(data.card_index,data.output_index);
                    status_queue.set_state(data.card_index,data.output_index,(s == 1) ? 0 : 1);
                }
                if (data.no_impuls) {
                    status_queue.set_state(data.card_index,data.output_index,data.status);
                }
            }

            // vor dem naechsten Schreibvorgang 15 Millisekunden warten
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    catch (std::exception& e) {
        msg.log("ERROR","Exception: " + std::string(e.what()));
    }
    catch (...) {
        msg.log("ERROR","Unbekannter Fehler.");
    }
}
// ----------------------------------------------------------------------------
/**
 * @brief Erzeugt eine HTTP-Antwort mit Headern.
 *
 * @param content Der Inhalt, der an den Browser gesendet wird.
 * @return Gibt eine vollstaendige HTTP-Antwort mit Headern zurueck.
 *
 * @author Andreas Jentsch
 * @date 20.06.2026
 */
std::string get_http_response(const std::string& content) {

    std::ostringstream res;
    res << "HTTP/1.1 200 OK\r\n" \
    << "Content-Type: application/json\r\n" \
    << "Content-Length: " << content.length() << "\r\n" \
    << "Connection: close\r\n" \
    << "\r\n" \
    << content;

    return res.str();
}


int main() {

    t_log msg; // ein Objekt der Klasse t_log erzeugen
    modbus_t* mb; // modbus RTU RS485 (einen Zeiger erzeugen)
    std::string logline; // zum Speichern einer Zeile im Log
    int card_count;

    try {

        // die RS232-Schnittstelle konfigurieren
        const auto device = shs_config.options.at("serial_device");
        const auto baud = std::stoi(shs_config.options.at("serial_baud"));
        const auto parity = shs_config.options.at("serial_parity")[0];
        const auto data_bit = std::stoi(shs_config.options.at("serial_data_bit"));
        const auto stop_bit = std::stoi(shs_config.options.at("serial_stop_bit"));

        // die Werte f. eine spaetere Ausgabe speichern
        logline = "Modbus RTU RS485: " + device + " " + std::to_string(baud) + " " + parity + " " + std::to_string(data_bit) + " " + std::to_string(stop_bit);

        // ein Modbus RTU Objekt erzeugen
        mb = modbus_new_rtu(device.c_str(), baud, parity, data_bit, stop_bit);
        if (mb == nullptr) {
            msg.log("ERROR","Es konnte keine Verbindung zu " + device + " hergestellt werden.");
            return 1;
        }

        // pruefen ob, mindestens eine Relais-Karte in der Konfig steht (card_count > 0)
        card_count = std::stoi(shs_config.options.at("card_count"));
        if (card_count == 0) {
            msg.log("WARNING","Es ist keine Relais-Karte konfiguriert.");
            msg.log("INFO","Programm beendet.");
            return 0;
        }
    }
    catch (std::exception& e) {
        msg.log("ERROR",e.what());
        return 1;
    }
    catch (...) {
        msg.log("ERROR","Unbekannter Fehler.");
        return 1;
    }

    // eine Modbus RTU RS485 Verbindung aufbauen
    if (modbus_connect(mb) == -1) {
        throw std::runtime_error(modbus_strerror(errno));
    }

    // verhindert einen Verbindungsabbruch nach dem unsauberen Beenden einer Verbindung
    if (int fd = modbus_get_socket(mb); fd != -1) {
        tcflush(fd, TCIOFLUSH); // leert Sende- und Empfangspuffer
    }
    msg.log("INFO","Modbus RTU RS485: Verbindung hergestellt.");
    msg.log("INFO",logline);

    // nach dem Aufbau der Verbindung 400 Millisekunden warten
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // den Antwort-Timeout auf 200 Millisekunden setzen
    int rc;
    {
        std::lock_guard<std::mutex> lock(mbus_lock);
        rc = modbus_set_response_timeout(mb, 0, 200000);
    }
    if (rc == -1) {
        msg.log("ERROR","Der Antwort-Timeout konnte nicht eingestellt werden.");
    }
    else {
        msg.log("INFO","Modbus RTU RS485: Antwort-Timeout wurde auf 200 Millisekunden eingestellt.");
    }

    // die Status-Queue initialisieren
    t_waveshare_card card(8,8);
    for (int i = 0; i < card_count; i++) {

        card.index = shs_config.waveshare.card[i].index;
        card.address = shs_config.waveshare.card[i].address;

        for (int j = 0; j < card.get_input_count(); j++) {
            card.inputs[j].type = shs_config.waveshare.card[i].input[j].type;
            card.outputs[j].index = j;
        }

        status_queue.cards.push_back(card);
    }

    for (int i = 0;i < 7;++i) {
        std::cout << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // threads starten
    std::thread _thread_web_requests(thread_tcp_server,std::ref(msg));
    std::thread _thread_mbus_h_inputs(thread_modbus_h_inputs,std::ref(msg),mb);
    std::thread _thread_mbus_h_outputs(thread_mbus_h_outputs,std::ref(msg),mb);

    if (_thread_web_requests.joinable()) {
        _thread_web_requests.join();
    }

    if (_thread_mbus_h_inputs.joinable()) {
        _thread_mbus_h_inputs.join();
    }

    if (_thread_mbus_h_outputs.joinable()) {
        _thread_mbus_h_outputs.join();
    }

    return 0;
}