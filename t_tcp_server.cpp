//
// Created by aj on 5/30/26.
//

#include "t_tcp_server.h"
#include <iostream>
#include <cstring>

t_tcp_server::t_tcp_server(const int port) : m_server_fd(-1), m_port(port), m_is_running(false) {}

t_tcp_server::~t_tcp_server() {
    stop();
}

bool t_tcp_server::start() {
    // 1. Server Socket erstellen
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) return false;

    // Port sofort wiederverwendbar machen (verhindert "Address already in use" Fehler)
    int opt = 1;
    setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. An Port und IP binden
    sockaddr_in server_addr{};
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Auf allen Netzwerkinterfaces lauschen
    server_addr.sin_port = htons(m_port);

    if (bind(m_server_fd, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        close(m_server_fd);
        return false;
    }

    // 3. In den Lauschmodus wechseln (Warteschlange für bis zu 10 Clients)
    if (listen(m_server_fd, 10) < 0) {
        close(m_server_fd);
        return false;
    }

    m_is_running = true;
    std::cout << "Server erfolgreich gestartet auf Port " << m_port << "..." << std::endl;

    // Hauptschleife zum Akzeptieren von Clients
    while (m_is_running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // Blockiert, bis ein Client connectet
        int client_fd = accept(m_server_fd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_len);

        if (client_fd < 0) {
            if (m_is_running) std::cerr << "Fehler beim Akzeptieren eines Clients!" << std::endl;
            break;
        }

        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        std::cout << "[Neu] Verbindung von " << client_ip << " akzeptiert." << std::endl;

        // Starte einen neuen Thread für diesen spezifischen Client
        m_client_threads.emplace_back(&t_tcp_server::client_handler, this, client_fd, client_ip
        );
    }

    return true;
}

void t_tcp_server::client_handler(int client_fd, const std::string& client_ip) const {

    char buffer[1024];

    while (m_is_running) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received <= 0) {
            // Client hat Verbindung getrennt oder Fehler
            std::cout << "[Trennen] Client " << client_ip << " hat die Verbindung geschlossen." << std::endl;
            break;
        }

        // 2. Den rohen HTTP-Request Zeile für Zeile durchgehen
        std::string raw_request(buffer);
        std::istringstream stream(raw_request);
        std::string line;

        while (std::getline(stream, line)) {
            // Das '\r' am Ende jeder HTTP-Zeile entfernen
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }


            // Suchen, wo die Parameter nach dem '?' beginnen
            size_t question_mark_pos = line.find('?');
            size_t space_pos = line.find(' ', question_mark_pos); // Ende der URL (vor HTTP/1.1)
            // Nach der echten IP-Adresse suchen
            if (line.rfind("GET ", 0) == 0) { // Beginnt die Zeile mit diesem Text?
                std::string query_string = line.substr(question_mark_pos + 1, space_pos - question_mark_pos - 1);
                std::cout << query_string << std::endl;
            }
        }

        std::string nachricht(buffer, bytes_received);
        std::cout << "[" << client_ip << "]: " << nachricht << std::endl;

        // Echo-Antwort: Schicke den Text einfach zurück
        std::string antwort = "Server empfing: " + nachricht;
        send(client_fd, antwort.c_str(), antwort.length(), 0);
    }

    close(client_fd);
}

void t_tcp_server::stop() {
    if (!m_is_running) return;

    m_is_running = false;

    if (m_server_fd != -1) {
        close(m_server_fd);
        m_server_fd = -1;
    }

    // Warte, bis alle Client-Threads sicher beendet wurden
    for (std::thread& th : m_client_threads) {
        if (th.joinable()) {
            th.join();
        }
    }
    m_client_threads.clear();
    std::cout << "Server sauber gestoppt." << std::endl;
}






// TEST
/*

#include "t_tcp_server.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
// 1. Server auf Port 8080 instanziieren
t_tcp_server server(8080);
// 2. Den Server in einem separaten Thread starten, damit main() nicht blockiert
std::thread server_thread([&server]() {
server.start();
});

// Dem Server einen kurzen Moment Zeit zum Starten geben
std::this_thread::sleep_for(std::chrono::milliseconds(500));

std::cout << "\n==================================================" << std::endl;
std::cout << "Server laeuft im Hintergrund." << std::endl;
std::cout << "Du kannst dich jetzt mit mehreren Clients verbinden!" << std::endl;
std::cout << "Druecke ENTER in diesem Terminal, um den Server zu beenden." << std::endl;
std::cout << "==================================================\n" << std::endl;

// 3. Warten, bis der Benutzer ENTER drückt
std::cin.get();

// 4. Server sauber herunterfahren
std::cout << "Fahre Server herunter..." << std::endl;
server.stop();

// Den Hintergrund-Thread sauber einsammeln
if (server_thread.joinable()) {
    server_thread.join();
    }

    std::cout << "Programm erfolgreich beendet." << std::endl;
    return 0;
    }
 */