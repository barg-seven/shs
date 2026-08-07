//
// Created by aj on 6/27/26.
//

#ifndef SHS_T_WEB_SOCKET_H
#define SHS_T_WEB_SOCKET_H

#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h> // Für htons (Netzwerk-Byte-Reihenfolge)



class t_web_socket {

public:
    t_web_socket();
    ~t_web_socket();

    std::vector<std::thread> ct; // client threads
    static int send_frame(int client, const std::string& json_text);
    void broadcast_relays_status(const std::string& json_text);
    static std::string get_client_key(std::istringstream& http_request);
    [[nodiscard]] std::string create_accept_key(const std::string& client_key) const;
    static void create_http_response_header(std::string &response, const std::string &accept_key);
    static bool web_socket_request(std::istringstream& http_request);
    void add_client(const int& client);
    void remove_client(const int& client);
    static std::string receive_frame(int client_fd, bool& verbindung_offen);

private:
    std::mutex _mtx;
    const std::string _ws_uuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // festgelegt in RFC 6455
    std::vector<int> _client_sockets;
    static uint32_t _rol(uint32_t value, size_t bits);
    static std::vector<uint8_t> _sha1(const std::string& input);
    static std::string _base64(const uint8_t* input, size_t length);
};


#endif //SHS_T_WEB_SOCKET_H
