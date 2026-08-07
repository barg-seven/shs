//
// Created by aj on 6/29/26.
//

#include "t_log.h"

t_log::t_log(const std::string& filename) {
    log_file_.open(filename, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "[Fehler] Logdatei konnte nicht geoeffnet werden!" << std::endl;
    }
}
t_log::~t_log() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void t_log::write(const std::string& level, const std::string& message, const std::string& file_info) {
    const std::string timestamp = get_timestamp();

    // Pfad bereinigen (nur "main.cpp:42" statt "/home/user/.../main.cpp:42")
    std::string clean_file = file_info;
    size_t last_slash = clean_file.find_last_of("\\/");
    if (last_slash != std::string::npos) {
        clean_file = clean_file.substr(last_slash + 1);
    }

    // 1. In die Datei schreiben
    if (log_file_.is_open()) {
        log_file_ << "[" << timestamp << "] [" << level << "] [" << clean_file << "] " << message << "\n";
        log_file_.flush();
    }

    // 2. Schön formatiert auf der Konsole ausgeben
    std::string color = "\033[0m";
    if (level == "INFO")    color = "\033[32m"; // Grün
    if (level == "WARNING") color = "\033[33m"; // Gelb
    if (level == "ERROR")   color = "\033[31m"; // Rot

    std::cout << "[" << timestamp << "] " << color << "[" << level << "]\033[0m "
              << "\033[90m[" << clean_file << "]\033[0m " << message << std::endl;
}