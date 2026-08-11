

#include "t_shs.h"
#include <sys/socket.h> // f. socket()
#include <fcntl.h>      // f. fcntl()
#include <netinet/in.h> // f. struct sockaddr_in
#include <unistd.h>     // f. close()
#include <poll.h>       // struct pollfd fds;
#include <iostream>
#include <sstream>
#include <iomanip>
#include <termios.h>    // f. tcflush()
//#include <pwd.h>        // f. getpwnam()
// ----------------------------------------------------------------------------
t_shs::t_shs() {
    _ss = -1;
    logfilename = "/var/log/shs.log";
}
// ----------------------------------------------------------------------------
t_shs::~t_shs()
{
    if (_ss >= 0) {
        close(_ss);
    }
}
// ----------------------------------------------------------------------------
unsigned int t_shs::modbus_set_response_timeout(modbus_t*& mbus,const unsigned int& ms)
{
    std::lock_guard<std::mutex> lock(_m_mbus);
    if (const int rc = ::modbus_set_response_timeout(mbus, 0, ms * 1000);rc == -1) {
        return 1;
    }
    return ms;
}
// ----------------------------------------------------------------------------
int t_shs::connect_to_serial_device(modbus_t*& mbus,std::string& str,const int& wait) const
{
    // die RS232-Schnittstelle konfigurieren
    const std::string device = config.options.at("serial_device");
    const int baud = std::stoi(config.options.at("serial_baud"));
    const char parity = config.options.at("serial_parity")[0];
    const int data_bit = std::stoi(config.options.at("serial_data_bit"));
    const int stop_bit = std::stoi(config.options.at("serial_stop_bit"));

    // ein Modbus RTU Objekt erzeugen
    mbus = modbus_new_rtu(device.c_str(), baud, parity, data_bit, stop_bit);
    if (mbus == nullptr) {
        return 1;
    }

    // eine Modbus RTU RS485 Verbindung aufbauen
    if (modbus_connect(mbus) == -1) {
        throw std::runtime_error(modbus_strerror(errno));
    }

    // verhindert einen Verbindungsabbruch nach dem unsauberen Beenden einer Verbindung
    if (const int fd = modbus_get_socket(mbus); fd != -1) {
        tcflush(fd, TCIOFLUSH); // leert Sende- und Empfangspuffer
    }

    str = "Modbus RTU RS485: " + device + " " + std::to_string(baud) + " " + parity + " " + std::to_string(data_bit) + " " + std::to_string(stop_bit);

    // nach dem Aufbau der Verbindung 400 Millisekunden warten
    std::this_thread::sleep_for(std::chrono::milliseconds(wait));

    return 0;
}
// ----------------------------------------------------------------------------
/**
 * @brief Schreibt auf Modbus RTU RS485
 * @details
 * Diese Threadfunktion schaltet die Relais auf den Relais-Karten. Ob es sich um einen
 * einfachen Schaltvorgang oder um einen Schaltimpuls handelt, wird von den andren
 * Threads per Thread-Queue gesteuert.
 *
 * @param mb Modbus RTU RS485 Zeiger.
 *
 * @author Andreas Jentsch
 * @date 18.06.2026
 */
void t_shs::thread_mbus_h_outputs(modbus_t*& mb)
{

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    pthread_setname_np(pthread_self(), MBUS_H_OUTPUTS_THREAD_NAME);

    try {

        int rc;
        t_command data = {};
        const auto t = "Thread " + std::string(MBUS_H_OUTPUTS_THREAD_NAME) + " ";

        while (!thread_mbus_h_outputs_stop) {

            // warten bis ein Thread push() aufruft oder der Timeout abgelaufen ist
            if (!cq.pop_with_timeout(data,0)) {
                continue;
            }

            // ein Relais schalten
            {
                std::lock_guard<std::mutex> lock(_m_mbus);

                // die Modbus-Adresse der Relais Karte festlegen
                modbus_set_slave(mb, data.card_address);

                // den neuen Status auf die Karte schreiben
                rc = modbus_write_bit(mb, data.output_address, data.status);
            }

            if (rc == -1) {
                log(t,"ERROR","Modbus RTU RS485: Funktion modbus_write_bit() Adresse " + std::to_string(1) + " Code " + std::string(modbus_strerror(errno)));
            }
            else {
                log(t,"INFO","Relais geschaltet Index " + std::to_string(data.output_address));
            }

            if (!data.dim) {

                // den neuen Status in die Status-Queue schreiben
                if (data.impuls) {
                    const int s = s_queue.get_state(data.card_index,data.output_index);
                    s_queue.set_state(data.card_index,data.output_index,(s == 1) ? 0 : 1);
                }
                if (data.no_impuls) {
                    s_queue.set_state(data.card_index,data.output_index,data.status);
                }
                sq.push(s_queue.cards);
            }

            // vor dem naechsten Schreibvorgang 15 Millisekunden warten
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    catch (std::exception& e) {
        log("","ERROR","Exception: " + std::string(e.what()));
    }
    catch (...) {
        log("","ERROR","Unbekannter Fehler.");
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
 * @param mb Modbus RTU RS485 Zeiger.
 *
 * @author Andreas Jentsch
 * @date 06.06.2026
 */
void t_shs::thread_modbus_h_inputs(modbus_t*& mb)
{

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    pthread_setname_np(pthread_self(), MBUS_H_INPUTS_THREAD_NAME);

    try {

        int rc; // return code
        t_command data{};
        std::ostringstream json_response; // die Nutzdaten

        while(!thread_mbus_h_inputs_stop) {

            // alle Relais-Karten durchlaufen und pruefen, ob an einem Eingang ein Signal anliegt
            for (auto & card : /* test: 001 status_queue.cards*/s_queue.cards) {

                // nimmt die 8 Zustaende von Eingaengen einer Relais Karte auf
                uint8_t inputs[8] = {0};

                {
                    std::lock_guard<std::mutex> lock(_m_mbus);
                    modbus_set_slave(mb, card.address); // die Modbus-Adresse der Relais Karte festlegen
                    rc = modbus_read_input_bits(mb, 0, 8, inputs);
                }

                if (rc == -1) {
                    log("","ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                    //throw std::runtime_error(modbus_strerror(errno));
                }

                // nach dem Lesen 15 Millisekunden warten
                std::this_thread::sleep_for(std::chrono::milliseconds(15));

                // zustaende aller 8 Digitalen Eingaenge der aktuellen Karte ermitteln
                for (int i = 0; i < 8; ++i) {

                    uint8_t status;
                    constexpr auto t = "Taster: ";

                    // wenn der Eingang den Status 1 hat (ein Taster wurde gedrueckt)
                    if (inputs[i] == 1 && card.inputs[i].type == "finder") {

                        // einen Eintrag in der Logdatei erzeugen
                        log(t,"INFO","Signal liegt an. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                        {
                            // den aktuellen Zustand des Relais (Outputs) auf der Relais-Karte ermitteln
                            std::lock_guard<std::mutex> lock(_m_mbus);
                            rc = modbus_read_bits(mb,i,1,&status);
                        }

                        if (rc == -1) {
                            log("","ERROR","Modbus RTU RS485: Funktion modbus_read_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                        }

                        // nach dem Lesen 15 Millisekunden warten
                        //std::this_thread::sleep_for(std::chrono::milliseconds(15));

                        status = (status == 0) ? 1 : 0; // invertieren (aus 0 wird 1 und aus 1 wird 0)

                        // befehl erzeugen und relais schalten
                        data.card_address = card.address;
                        data.output_index = i;
                        data.output_address = i;
                        data.status = status;
                        data.no_impuls = true;
                        cq.push(data);

                        // solange warten bis der Taster losgelassen wurde
                        do {

                            // warten, dann pruefen ob der Taster losgelassen wurde
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            {
                                std::lock_guard<std::mutex> lock(_m_mbus);
                                rc = modbus_read_input_bits(mb, i, 1, &status);
                            }
                            if (rc == -1) {
                                log("","ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Adresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                            }
                        } while (status == 1);

                        log(t,"INFO","Signal weg. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));
                    }


                    if (inputs[i] == 1 && card.inputs[i].type == "eltako") {

                        log(t,"INFO","Signal liegt an. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                        // befehl erzeugen
                        data.card_address = card.address;
                        data.output_index = i;
                        data.output_address = i;

                        // warten dann pruefen ob der Taster losgelassen wurde
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        {
                            std::lock_guard<std::mutex> lock(_m_mbus);
                            rc = modbus_read_input_bits(mb, i, 1, &status);
                        }
                        if (rc == -1) {
                            log("","ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Kartenadresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                        }

                        // der Taster wurde losgelassen, den Eltako mit Impuls schalten
                        if (status == 0) {

                            data.status = 1; // nur den Status anpassen
                            cq.push(data);

                            // f. den Impuls
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));

                            data.status = 0; // nur den Status anpassen
                            cq.push(data);

                            log(t,"INFO","Schaltimpuls gesendet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));
                        }
                        else { // den Dim-Vorgang starten

                            log(t,"INFO","Dim-Vorgang gestartet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));

                            // den Dimm-Vorgang starten
                            data.status = 1; // nur den Status anpassen
                            cq.push(data);

                            // solange warten bis der Taster losgelassen wurde
                            do {

                                // ist der Status 0, wurde der Taster losgelassen
                                {
                                    std::lock_guard<std::mutex> lock(_m_mbus);
                                    rc = modbus_read_input_bits(mb, i, 1, &status);
                                }
                                if (rc == -1) {
                                    log("","ERROR","Modbus RTU RS485: Funktion modbus_read_input_bits() Kartenadresse " + std::to_string(card.address) + " Code " + std::string(modbus_strerror(errno)));
                                }
                            } while (status == 1);

                            log(t,"INFO","Dim-Vorgang beendet. Kartenadresse " + std::to_string(card.address) + " Ausgang Index " + std::to_string(i));
                            log(t,"INFO","Signal weg. Kartenadresse " + std::to_string(card.address) + " Eingang Index " + std::to_string(i));

                            // das Relais abschalten
                            data.status = 0; // nur den Status anpassen
                            cq.push(data);
                        }
                    }

                    // die Status-Queue aktualisieren
                    if (sq.pop_with_timeout(s_queue.cards,0)) {
                        s_queue.as_json(json_response);
                        _ws.broadcast_relays_status(json_response.str());
                    }
                }
            }
        }
    }
    catch (std::exception& e) {
        log("","ERROR","Exception: " + std::string(e.what()));
    }
    catch (...) {
        log("","ERROR","Exception: Unbekannter Fehler.");
    }
}
// ----------------------------------------------------------------------------
/**
 * @brief TCP-Server Thread.
 * @details Diese Methode ist der TCP-Server. Er nimmt TCP-Verbindungen entgegen und
 * erstellt f. jeden TCP-Client einen neuen Thread. Als zweite Aufgabe prueft er,
 * ob die Konfiguration erneut geladen werden kann.
 *
 * @param stop_token Der Server wird beendet.
 * @param port Die Portnummer des TCP-Servers.
 *
 * @author Andreas Jentsch
 * @date 26.07.2026
 */
void t_shs::thread_tcp_server(const std::stop_token& stop_token,const int& port) {

    // dem Thread einen Namen geben
    // unter Linux kann man den Thread mit diesem Befehl beobachten
    // top -H -p $(pgrep shs)
    // ps H -C shs -o 'pid tid cmd comm'
    pthread_setname_np(pthread_self(), TCP_SERVER_THREAD_NAME);

    constexpr auto t = "TCP-Server: ";

    // socket erstellen
    if (_ss = socket(AF_INET, SOCK_STREAM, 0); _ss == -1) {
        log(t,"ERROR","Code " + std::to_string(errno) + ": " + std::system_category().message(errno));
        return;
    }

    // port sofort wiederverwendbar machen
    constexpr int opt = 1;
    setsockopt(_ss, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // socket auf NON-BLOCKING setzen
    const int flags = fcntl(_ss, F_GETFL, 0);
    fcntl(_ss, F_SETFL, flags | O_NONBLOCK);

    // bind & listen
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(_ss, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        log(t,"ERROR","Code " + std::to_string(errno) + ": " + std::system_category().message(errno));
        return;
    }
    if (listen(_ss, 5) < 0) {
        log(t,"ERROR","Code " + std::to_string(errno) + ": " + std::system_category().message(errno));
        return;
    }

    // pollfd Struktur initialisieren
    struct pollfd fds = {0};
    fds.fd = _ss;
    fds.events = POLLIN; // auf eingehende Verbindungen lauschen

    //log(t,"INFO",TCP_LISTEN_ON + std::to_string(port));

    while (!stop_token.stop_requested()) {

        // prueft, ob die Konfiguration erneut geladen werden soll
        if (t_control_cmd c(-1); ccq.pop_with_timeout(c,0)) {
            std::lock_guard<std::mutex> lock(_m_config);
            config.reload();
        }

        // wartet maximal 70 Millisekunden auf ein Event
        const int rc = poll(&fds, 1, 70);

        if (rc == -1) {
            if (errno == EINTR) continue; // durch Signal unterbrochen
            log(t,"ERROR","Code " + std::to_string(errno) + ": " + std::system_category().message(errno));
            break;
        }

        if (rc == 0) continue; // timeout

        // wenn POLLIN gesetzt ist, wartet ein Client im Listen-Backlog
        if (fds.revents & POLLIN) {

            struct sockaddr_in client_address {};
            socklen_t client_len = sizeof(client_address);

            // die IP-Adresse in einen lesbaren String umwandeln, ist immer 127.0.0.1 (wegen Reverse Proxy)
            //std::string client_ip = inet_ntoa(client_address.sin_addr);

            if (const int cs = accept(_ss, reinterpret_cast<struct sockaddr *>(&client_address), &client_len); cs >= 0) {
                std::jthread(&t_shs::thread_handle_http_request, this, cs).detach();
            }
            else {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    log(t,"WARNING","Code " + std::to_string(errno) + ": " + std::system_category().message(errno));
                }
            }
        }
    }
    close(_ss);
    _ss = -1;
}
// ----------------------------------------------------------------------------
void t_shs::thread_handle_http_request(const int& cs) {

    char buffer[1024] = {};
    constexpr auto t = "HTTP-Server: ";

    // den HTTP-Request vom Browser einlesen
    if (const ssize_t bytes_received = recv(cs, buffer, sizeof(buffer) - 1, 0); bytes_received > 0) {

        std::istringstream request(std::string(buffer,bytes_received)); // der HTTP-Request im Stream
        std::string http_response; // der Antwort-Header
        std::ostringstream json_response; // die Nutzdaten

        if (t_web_socket::web_socket_request(request)) {

            const std::string client_key = t_web_socket::get_client_key(request);
            const std::string accept_key = _ws.create_accept_key(client_key);

            // eine Handshake-Antwort an den Browser senden
            t_web_socket::create_http_response_header(http_response, accept_key);
            send(cs, http_response.c_str(), http_response.length(), 0);

            // den Client f. Push-Benachrichtigungen speichern
            _ws.add_client(cs);

            log(t,"INFO","Client " + std::to_string(cs) + " hat eine Web-Socket-Verbindung hergestellt.");

            // die ganze Status-Queue an den Client senden
            //status_queue.as_json(json_response); // test: 001
            if (const long rc = t_web_socket::send_frame(cs,json_response.str()); rc < 0) {
                log(t,"ERROR","Senden der Status-Queue fehlgeschlagen.");
            }

            // warten bis der Browser die Verbindung trennt
            bool connected = true;

            while (connected) {

                // blockiert und wartet, bis der Browser einen Befehl schickt
                std::string json = t_web_socket::receive_frame(cs, connected);

                if (!connected) {
                    break; // browser hat geschlossen oder Netzwerkfehler
                }

                if (!json.empty()) {

                    log(t,"INFO","Client " + std::to_string(cs) + " sendet Befehl: " + json);

                    std::map<std::string,std::string> result = t_shs::get_command(json);

                    t_command cmd = {};

                    {
                        std::lock_guard<std::mutex> lock(_m_config);
                        cmd.action = std::stoi(result.at("action"));
                        cmd.card_index = config.card.at(std::stoi(result.at("card"))).index;
                        cmd.card_address = config.card.at(std::stoi(result.at("card"))).address;
                        cmd.output_index = std::stoi(result.at("relay"));
                        cmd.output_address = std::stoi(result.at("relay"));
                        cmd.status = std::stoi(result.at("state"));
                    }

                    switch (cmd.action) {
                        case 0: {

                            // f. das Schreiben in die Status-Queue
                            cmd.dim = false;
                            cmd.impuls = true;

                            cmd.status = 1; // den Status ueberschreiben
                            cq.push(cmd);

                            // f. den Impuls
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));

                            cmd.impuls = false; // f. das Schreiben in die Status-Queue

                            cmd.status = 0; // den Status ueberschreiben
                            cq.push(cmd);
                            break;
                        }
                        case 1: {

                            cmd.dim = true;
                            cmd.status = 1; // den Status ueberschreiben

                            cq.push(cmd);
                            break;
                        }
                        case 2: {

                            cmd.dim = true;
                            cmd.status = 0; // den Status ueberschreiben

                            cq.push(cmd);
                            break;
                        }
                        case 3: {

                            // f. das Schreiben in die Status-Queue
                            cmd.dim = false;
                            cmd.impuls = false;
                            cmd.no_impuls = true;

                            cq.push(cmd);
                            break;
                        }
                        default: {}
                    }

                    // dem Thread mbus-h-outputs Zeit geben um die Status-Queue zu aktualisieren
                    // todo Eventuell eine Queue?
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    //status_queue.as_json(json_response); // test: 001
                    _ws.broadcast_relays_status(json_response.str());
                }
            }

            log(t,"INFO","Client " + std::to_string(cs) + " hat die Web-Socket-Verbindung getrennt.");

            // wenn die Schleife beendet ist
            _ws.remove_client(cs);
            close(cs);
        }
        // ein HTTP-Request f. eine Datei oder Daten
        // *****************************************
        else {

            if (vic_metrics_request(request)) {

                // mbus abfragen & das ergebnis senden

                http_response = "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "# HELP kueche_feuchtigkeit_prozent Aktuelle Luftfeuchtigkeit\n"
                "# TYPE kueche_feuchtigkeit_prozent gauge\n"
                "kueche_feuchtigkeit_prozent{sensor_id=\"01\",raum=\"Kueche\"} 48.2\n";
                send(cs, http_response.c_str(), http_response.length(), 0);
            }
            close(cs);
        }
    }
}
// ----------------------------------------------------------------------------
/**
 * @brief Zerlegt den Befehl, und speichert diesen in einem Map-Container.
 * @details
 *
 * @param json Enthaelt die Daten, die der Client gesendet hat. Ein Befehl zum Schalten.
 * @return Gibt den Befehlt als Map-Container zurueck.
 *
 * @author Andreas Jentsch
 * @date 01.08.2026
 */
std::map<std::string, std::string> t_shs::get_command(const std::string& json)
{
    std::map<std::string, std::string> result;

    // die drei Schluessel, nach denen wir suchen
    std::vector<std::string> keys = {"action","card", "relay", "state"};

    for (const auto& key : keys)
    {
        // suche nach dem Schluessel im JSON (z.B. "card")
        const size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) continue; // schluessel nicht gefunden -> ueberspringen

        // suche den dazugehoerigen Doppelpunkt nach dem Schluessel
        const size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) continue;

        // finde den Start des Wertes
        const size_t valStart = json.find_first_not_of(" \t", colonPos + 1);
        if (valStart == std::string::npos) continue;

        // finde das Ende des Wertes (entweder am Komma oder an der schliessenden Klammer '}')
        const size_t valEnd = json.find_first_of(",}", valStart);
        if (valEnd == std::string::npos) continue;

        // wert ausschneiden und in die Map einfuegen
        const std::string val = json.substr(valStart, valEnd - valStart);
        result[key] = val;
    }

    return result;
}
// ----------------------------------------------------------------------------
bool t_shs::vic_metrics_request(std::istringstream& http_request)
{
    std::string line;

    // vor dem Lesen Fehlerbits und Positionszeiger zuruecksetzen
    http_request.clear();
    http_request.seekg(0);

    while (std::getline(http_request, line)) {

        if (line.find("GET /metrics") != std::string::npos) {
            return true;
        }
    }
    return false;
}
// ----------------------------------------------------------------------------
/**
 * @brief Schreibt Nachrichten in eine Datei.
 * @details Diese Methode schreibt Nachrichten in eine Datei und in die
 * Standardausgabe. Wenn die Eigenschaft t_log::debug den Wert true hat, werden die Namen
 * der Unit-Dateien und Zeilennummern ausgegeben.
 *
 * @param type s
 * @param level s
 * @param message s
 * @param debuginfo s
 */
void t_shs::write(const std::string& type,const std::string& level, const std::string& message,const std::string& debuginfo)
{
    std::lock_guard<std::mutex> lock(_m_log);

    _logfile.open(logfilename, std::ios::app);
    if (!_logfile.is_open()) {
        return;
    }

    std::string df = debuginfo;
    const std::string timestamp = get_timestamp();

    if (this->_debug) {
        // pfad bereinigen (nur "main.cpp:42" statt "/home/user/.../main.cpp:42")
        if (const size_t last_slash = df.find_last_of("\\/"); last_slash != std::string::npos) {
            df = df.substr(last_slash + 1);
        }

        _logfile << "[" << timestamp << "] [" << level << "] [" << df << "] " << type << message << "\n";
    }
    else {
        _logfile << "[" << timestamp << "] [" << level << "] " << type << message << "\n";
    }
    _logfile.flush();

    // formatiert auf der Konsole ausgeben
    std::string color = "\033[0m";
    if (level == "INFO")    color = "\033[32m"; // gruen
    if (level == "WARNING") color = "\033[33m"; // gelb
    if (level == "ERROR")   color = "\033[31m"; // rot

    std::cout << "[" << timestamp << "] " << color << "[" << level << "]\033[0m " << "\033[90m[" << df << "]\033[0m " << type << message << std::endl;
}
// ----------------------------------------------------------------------------
// hilfsfunktion fuer den aktuellen Zeitstempel (YYYY-MM-DD HH:MM:SS)
std::string t_shs::get_timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------