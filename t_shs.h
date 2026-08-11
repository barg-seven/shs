
#ifndef SHS_T_SHS_H
#define SHS_T_SHS_H

#include <vector>
#include <stop_token>
#include <thread>
#include <mutex>
#include <modbus/modbus.h>
#include <atomic>
#include "t_config.h"
#include "t_queue.h"
#include "t_web_socket.h"
#include "t_status_queue.h"

#define NAMELEN 16      // maximale Anzahl f. einen Threadnamen \0
#define TCP_SERVER_THREAD_NAME "tcp-server"
#define MBUS_H_INPUTS_THREAD_NAME "mbus-h-inputs"
#define MBUS_H_OUTPUTS_THREAD_NAME "mbus-h-outputs"

struct t_command;
struct t_control_cmd;

class t_shs {

    public:
    t_shs();
    ~t_shs();

    t_config config;
    t_queue<t_command> cq; // cq = command queue (relay schalten)
    t_queue<t_control_cmd> ccq; // ccq = control command queue (signale verarbeiten)
    t_status_queue s_queue; // s_queue = status queue
    t_queue<t_sq> sq; // sq = status queue
    std::string logfilename;
    std::stop_token tcp_server_stop;
    std::atomic<bool> thread_mbus_h_inputs_stop = false;
    std::atomic<bool> thread_mbus_h_outputs_stop = false;

    unsigned int modbus_set_response_timeout(modbus_t*& mbus,const unsigned int& ms);
    int connect_to_serial_device(modbus_t*& mbus,std::string& str,const int& wait) const;
    void thread_mbus_h_outputs(modbus_t*& mb);
    void thread_modbus_h_inputs(modbus_t*& mb);
    void thread_tcp_server(const std::stop_token& stop_token,const int& port);
    void thread_handle_http_request(const int& cs); // cs = client socket
    static std::map<std::string,std::string> get_command(const std::string& json);
    static bool vic_metrics_request(std::istringstream& http_request);
    void write(const std::string& type,const std::string& level, const std::string& message,const std::string& debuginfo);
    static std::string get_timestamp();

    private:
    int _ss; // server socket
    t_web_socket _ws = t_web_socket();
    std::vector<std::jthread> _ct; // client threads
    std::mutex _m_config;
    std::ofstream _logfile;
    std::mutex _m_log;
    std::mutex _m_mbus;
    bool _debug = true;
};

/**
 * @brief Diese Daten werden in eine Queue geschrieben.
 *
 * @details
 * Das sind die Daten, die zum Schalten eines Relais gebraucht werden. Sie werden ueber
 * die Thread-Queue an die Threads gegeben.
 * - dim            = zeigt ob gedimmt wird oder nicht
 * - card_index     = der Index der Relais-Karte
 * - card_address   = die Modbus Adresse der Karte (nicht der Index)
 * - output_index   = der Index eines Relais auf der Karte
 * - output_address = die Adresse des Relais 0 - 7
 * - status         = enthaelt den Wert, der geschrieben wird 0 oder 1
 * - action         = 0 = switch-eltako, 1 = dim-eltako-start, 2 = dim-eltako-stop, 3 = switch-relay
 *
 * @author Andreas Jentsch
 * @date 31.05.2026
 */
struct t_command {
    bool dim;
    int card_index;
    int card_address;
    int output_index;
    int output_address;
    uint8_t status;
    bool impuls;
    bool no_impuls;
    int action;
};

struct t_control_cmd {
    int cmd; // 0 = shutdown, 1 = reload config
    explicit t_control_cmd(const int& command) : cmd(-1) {
        cmd = command;
    }
};

#define log(type,level,message) write(type,level,message,std::string(__FILE__) + ":" + std::to_string(__LINE__))


#endif //SHS_T_SHS_H