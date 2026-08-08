//
// Created by aj on 7/11/26.
//

#include "t_config.h"
#include <iostream>
#include <algorithm>    // f. remove_if()
#include <charconv>     // std::from_chars()

t_config::t_config() = default;
// ----------------------------------------------------------------------------
t_config::~t_config() = default;
// ----------------------------------------------------------------------------
/**
 * @brief Leert, und liest die Konfig erneut ein.
 * @details Diese Methode leert die aktuell geladenen Konfig, und liest diese erneut
 * ein. Aufgerufen wird diese Methode wenn das Signal SIGHUP rein kommt.
 *
 * @author Andreas Jentsch
 * @date 25.07.2026
 */
void t_config::reload() {

    card.clear();
    parse(_config_file);
}
// ----------------------------------------------------------------------------
/**
 * @brief Liest die Konfig ein und speichert diese in einem Vector.
 *
 * @param file Die Konfigurationsdatei des Programms.
 *
 * @author Andreas Jentsch
 * @date 25.07.2026
 */
void t_config::parse(const std::string& file) {

    _file.open(file);
    _config_file = file;

    if (!_file.is_open()) {
        throw std::runtime_error("[Fehler] Konfigurationsdatei konnte nicht geoeffnet werden!");
    }

    int ci = 0; // ci = card index
    std::string line;

    // als Erstes alle Karten im Vector als Objekt speichern, ohne Inputs
    // ******************************************************************
    while (std::getline(_file, line)) {

        _trim(line);

        // leere Zeilen und Kommentare ueberspringen
        if (line.empty() || line[0] == '#') continue;

        // nach dem Gleichheitszeichen in der Zeile suchen
        if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {

            // den Schluessel und Wert voneinander trennen
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);

            _trim(key);
            _trim(value);

            // nach card_x_address suchen
            if (int n; key.rfind("card_") == 0 && key.length() > 4 && _try_parse_int(key[5],n)) {

                if (key.find("address") != std::string::npos && _try_parse_int(value,n)) {

                    // ein Objekt von t_card erzeugen
                    t_card c;
                    c.in.resize(8);

                    c.index = ci++;
                    c.address = n;
                    card.push_back(c);
                }
            }
        }
    }

    _file.clear();
    _file.seekg(0, std::ios::beg);

    // jetzt alle anderen Optionen aus der Datei im Vector speichern
    // *************************************************************
    while (std::getline(_file, line)) {

        _trim(line);

        // leere Zeilen und Kommentare ueberspringen
        if (line.empty() || line[0] == '#') continue;

        // nach dem Gleichheitszeichen in der Zeile suchen
        if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {

            // den Schluessel und Wert voneinander trennen
            std::string key = line.substr(0, delimiter_pos);
            std::string value = line.substr(delimiter_pos + 1);

            _trim(key);
            _trim(value);

            // hier werden diese Optionen verarbeitet: card_0_input_0_type
            if (int tmp_i; key.rfind("card_", 0) == 0 && _try_parse_int(key[5],tmp_i)) {
                if (key.length() > 4) {
                    auto tmp_s = std::string(1,key[5]);
                    if (int card_n; _try_parse_int(tmp_s,card_n)) {
                        tmp_s = std::string(1,key[13]);
                        if (int input_n; _try_parse_int(tmp_s,input_n)) {
                            for (auto& c : card) {
                                for (int i = 0;i < 8;++i) {
                                    c.in.at(i).index = i;
                                    c.in.at(i).address = i;
                                    c.in.at(i).type = value;
                                }
                            }
                        }
                    }
                }
                else {
                    options.insert(std::make_pair(key, value));
                }
            }
            // hier die anderen Optionen
            else {
                options.insert(std::make_pair(key, value));
            }
        }
    }

    _file.close();
}
// ----------------------------------------------------------------------------
/**
 * @brief Entfernt Leer- und Steuerzeichen.
 *
 * @details Entfernt Leer- und Steuerzeichen am Anfang und am Ende einer Zeichenkette.
 *
 * @param str Eine Zeichenkette
 * @return std::string
 *
 * @author Andreas Jentsch
 * @date 30.05.2026
 */
std::string t_config::_trim(std::string& str) {
    const auto new_end = std::ranges::remove_if(str, [](const unsigned char ch) {
        return std::isspace(ch);
    }).begin();
    str.erase(new_end, str.end());

    return str;
}
// ----------------------------------------------------------------------------
/**
 * @brief Versucht eine Zeichenkette in eine Zahl zu konvertieren.
 *
 * @param str Eine Zeichenkette.
 * @param out Der Wert von str als Ganzzahl.
 *
 * @return Wenn erfolgreich konvertiert wurde, wird true ansonsten false zurueckgegeben.
 *
 * @author Andreas Jentsch
 * @date 24.06.2026
 */
bool t_config::_try_parse_int(const std::string& str, int& out) {

    const char* start = str.data();
    const char* end = start + str.size();

    // versucht die Konvertierung
    auto [ptr, ec] = std::from_chars(start, end, out);

    // wenn kein Fehlercode (ec) vorliegt UND der gesamte String gelesen wurde,
    // war die Konvertierung zu 100% erfolgreich
    return (ec == std::errc() && ptr == end);
}
// ----------------------------------------------------------------------------
/**
 * @brief Ueberladen.
 *
 * @param ch Eine Zeichenkette.
 * @param out Der Wert von str als Ganzzahl.
 *
 * @return Wenn erfolgreich konvertiert wurde, wird true ansonsten false zurueckgegeben.
 */
bool t_config::_try_parse_int(const char ch, int& out) {
    return _try_parse_int(std::string{ch}, out);
}
// ----------------------------------------------------------------------------