//
// Created by aj on 6/29/26.
//

#ifndef SHS_T_LOG_H
#define SHS_T_LOG_H

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>

class t_log {

public:
    explicit t_log(const std::string& filename = "/var/log/shs.log");
    ~t_log();

    void write(const std::string& level, const std::string& message, const std::string& file_info = "");

private:
    std::ofstream log_file_;

    // Hilfsfunktion für den aktuellen Zeitstempel (YYYY-MM-DD HH:MM:SS)
    static std::string get_timestamp() {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};
#define log(level, message) write(level, message, std::string(__FILE__) + ":" + std::to_string(__LINE__))


#endif //SHS_T_LOG_H
