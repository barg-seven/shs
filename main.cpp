
#include "t_shs.h"
#include <csignal>      // f. SIGINT
#include <functional>   // f. bind_front()

t_shs shs;
static std::jthread tcp_server_thread;
static void handle_signals(int sig);


std::mutex http_response_lock;
//t_status_queue status_queue;
std::mutex web_clients_lock;

int main() {

    // einige Signale abfangen
    struct sigaction sa = {};
    sa.sa_handler = handle_signals;
    sigaction(SIGINT, &sa, nullptr); // Strg+C
    sigaction(SIGTERM, &sa, nullptr); // systemctl stop
    sigaction(SIGHUP, &sa, nullptr); // systemctl reload: ExecReload=/bin/kill -HUP $MAINPID

    modbus_t* mb; // modbus RTU RS485 (einen Zeiger erzeugen)
    std::string logline; // zum Speichern einer Zeile im Log

    constexpr auto t = "CCCCCCCCCCCCC: ";

    try {

        shs.config.parse("/etc/shs.conf");
        if (const int rc = shs.connect_to_serial_device(mb,logline,400); rc != 0) {
            shs.log(t,"ERROR","Es konnte keine Verbindung zu " + shs.config.options.at("serial_device") + " hergestellt werden.");
            return 1;
        }

        shs.log("","INFO","Modbus RTU RS485: Verbindung hergestellt.");
        shs.log("","INFO",logline);

        // den Antwort-Timeout auf 200 Millisekunden setzen
        if (constexpr unsigned int timeout = 200000; shs.modbus_set_response_timeout(mb,timeout) > 1) {
            shs.log("","INFO","Modbus RTU RS485: Antwort-Timeout wurde auf " + std::to_string(timeout / 1000) + " Millisekunden eingestellt.");
        }

        // die Status-Queue initialisieren
        t_waveshare_card card(8,8);
        for (int i = 0; i < std::stoi(shs.config.options.at("card_count")); i++) {

            card.index = shs.config.card[i].index;
            card.address = shs.config.card[i].address;

            for (int j = 0; j < card.get_input_count(); j++) {
                card.inputs[j].type = shs.config.card[i].in[j].type;
                card.outputs[j].index = j;
            }

            shs.s_queue.cards.push_back(card);
        }

        // threads starten
        tcp_server_thread = std::jthread(std::bind_front(&t_shs::thread_tcp_server,&shs),8080);
        std::thread _thread_mbus_h_inputs(&t_shs::thread_modbus_h_inputs,&shs,std::ref(mb));
        std::thread _thread_mbus_h_outputs(&t_shs::thread_mbus_h_outputs,&shs,std::ref(mb));

        if (tcp_server_thread.joinable()) {
            tcp_server_thread.join();
        }

        if (_thread_mbus_h_inputs.joinable()) {
            _thread_mbus_h_inputs.join();
        }

        if (_thread_mbus_h_outputs.joinable()) {
            _thread_mbus_h_outputs.join();
        }
    }
    catch (std::exception& e) {
        shs.log("","ERROR",e.what());
        return 1;
    }
    catch (...) {
        shs.log("","ERROR","Unbekannter Fehler.");
        return 1;
    }

    return 0;
}
//-----------------------------------------------------------------------------
/**
 * @brief Wird aufgerufen wenn ein Signal rein kommt.
 * @details Diese Funktion wird aufgerufen wenn ein Signal rein kommt. Die folgenden
 * Signale werden abgefangen.
 * - SIGINT
 * - SIGTERM
 * - SIGHUP
 *
 * @param sig Das Signal.
 *
 * @todo Es wird noch kein Thread wieder gestartet.
 * @todo Es werden noch nicht alle Threads sauber beendet.
 * @todo Die Serial Schnittstelle wird noch nicht erneut konfiguriert.
 */
void handle_signals(const int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM: { // Strg+C || systemctl stop
            tcp_server_thread.request_stop();
            break;
        }
        case SIGHUP: {
            shs.ccq.push(t_control_cmd(1));
            break;
        }
        default: {
            break;
        }
    }
}
//-----------------------------------------------------------------------------