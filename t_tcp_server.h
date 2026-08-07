//
// Created by aj on 5/30/26.
//

#ifndef SHS_T_TCP_SERVER_H
#define SHS_T_TCP_SERVER_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

class t_tcp_server {
private:
    int m_server_fd;
    int m_port;
    std::atomic<bool> m_is_running;
    std::vector<std::thread> m_client_threads;

    // Diese Funktion läuft für jeden Client in einem eigenen Thread
    void client_handler(int client_fd, const std::string& client_ip) const;

public:
    explicit t_tcp_server(int port);
    ~t_tcp_server(); // Beendet den Server sauber

    bool start();
    void stop();
};


#endif //SHS_T_TCP_SERVER_H
