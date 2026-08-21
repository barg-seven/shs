//
// Created by aj on 7/11/26.
//

#include "t_config.h"
#include <iostream>
#include <algorithm>    // f. remove_if()
#include <charconv>     // std::from_chars()
#include <sstream>

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

    std::string line;

    while (std::getline(_file, line)) {

        // leere Zeilen und Kommentare ueberspringen
        if (line.empty() || line[0] == '#') continue;

        std::string key,value;

        // nach [main] suchen
        // ********************************************************************
        if (line.find(SECTION_MAIN) != std::string::npos) {
            while (std::getline(_file, line)) {
                _trim_left(line);
                _trim_right(line);

                if (line[0] == '#') continue; // kommentare ueberspringen
                if (line.empty()) break; // eine leere Zeile markiert das Blockende

                // nach dem Gleichheitszeichen in der Zeile suchen
                if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {
                    // den Schluessel und Wert voneinander trennen
                    key = line.substr(0, delimiter_pos);
                    value = line.substr(delimiter_pos + 1);

                    _trim(key);
                    _trim_left(value);
                    _trim_right(value);
                }

                // serial
                if (line.rfind("serial_", 0) == 0) {
                    options.insert(std::make_pair(key, value));
                }

                // logfile
                if (line.rfind("logfile", 0) == 0) {
                    options.insert(std::make_pair(key, value));
                }
            }
        }

        // nach [card] suchen
        // ********************************************************************
        if (line.find("[card]") != std::string::npos) {
            t_card c;
            while (std::getline(_file, line)) {

                if (line[0] == '#') continue;

                if (line.empty()) break;

                // nach dem Gleichheitszeichen in der Zeile suchen
                if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {

                    // den Schluessel und Wert voneinander trennen
                    key = line.substr(0, delimiter_pos);
                    value = line.substr(delimiter_pos + 1);

                    _trim(key);
                    _trim(value);

                    //options.insert(std::make_pair(key, value));
                }

                // id
                if (int id;key.find(CARD_ID) != std::string::npos && _try_parse_int(value,id)) {
                    if (id == -1) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Karten ID -1");
                    }c.id = id;
                }

                // address
                if (int address;key.find(CARD_ADDRESS) != std::string::npos && _try_parse_int(value,address)) {
                    if (address == -1) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Karten Adresse -1");
                    }
                    c.address = address;
                }

                // inputs
                if (int inputs;key.find(CARD_INPUTS) != std::string::npos && _try_parse_int(value,inputs)) {
                    if (inputs == -1) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Anzahl Inputs -1");
                    }
                    c.in.resize(inputs);
                    for (int i = 0;i < inputs;++i) {
                        c.in.at(i).index = i;
                        c.in.at(i).address = i;
                    }
                }

                // outputs
                if (int outputs;key.find(CARD_OUTPUTS) != std::string::npos && _try_parse_int(value,outputs)) {
                    if (outputs == -1) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Anzahl Outputs -1");
                    }
                    c.out.resize(outputs);
                    for (int i = 0;i < outputs;++i) {
                        c.out.at(i).index = i;
                        c.out.at(i).address = i;
                    }
                }

                // disabled
                if (key.find(CARD_DISABLED) != std::string::npos) {
                    c.disabled = value == "true";
                }

                // input_types
                if (key.find(CARD_INPUT_TYPES) != std::string::npos) {
                    if (!value.empty() && value.front() == '[') value.erase(0,1);
                    if (!value.empty() && value.back() == ']') value.pop_back();

                    int i = 0;
                    std::string s;
                    std::stringstream ss(value);

                    while (std::getline(ss, s, ',')) {
                        c.in.at(i).type = s;
                        ++i;
                    }

                    if (c.in.size() != i) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Anzahl Input Types");
                    }
                }

                // input_broken
                if (key.find(CARD_INPUT_BROKEN) != std::string::npos) {
                    if (!value.empty() && value.front() == '[') value.erase(0,1);
                    if (!value.empty() && value.back() == ']') value.pop_back();

                    int i = 0;
                    std::string s;
                    std::stringstream ss(value);

                    while (std::getline(ss, s, ',')) {
                        c.in.at(i).broken = s == "true";
                        ++i;
                    }

                    if (c.in.size() != i) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Anzahl Input Broken");
                    }
                }

                // output_broken
                if (key.find(CARD_OUTPUT_BROKEN) != std::string::npos) {
                    if (!value.empty() && value.front() == '[') value.erase(0,1);
                    if (!value.empty() && value.back() == ']') value.pop_back();

                    int i = 0;
                    std::string s;
                    std::stringstream ss(value);

                    while (std::getline(ss, s, ',')) {
                        c.out.at(i).broken = s == "true";
                        ++i;
                    }

                    if (c.in.size() != i) {
                        throw std::runtime_error("Konfiguration Syntax Fehler: Anzahl Output Broken");
                    }
                }
            }
            card.emplace_back(c);
        }

        // nach [sensor] suchen
        if (line.rfind("[sensor]", 0) == 0) {
            t_sensor s;
            t_metric metric;

            // wird beendet, wenn eine leere Zeile gefunden wurde
            while (std::getline(_file, line)) {
                _trim_left(line);
                _trim_right(line);

                if (line[0] == '#') continue;

                if (line.empty()) break;

                if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {
                    key = line.substr(0, delimiter_pos);
                    value = line.substr(delimiter_pos + 1);
                }

                _trim_left(value);
                _trim_right(value);

                if (key.rfind(SENSOR_OPTION_ID, 0) == 0) {
                    if (s.id = _id(key,value); s.id == -1) {
                        throw std::runtime_error("Syntax Error Sensor-ID: " + std::to_string(s.id));
                    }
                }
                if (key.rfind(SENSOR_OPTION_NAME, 0) == 0) {
                    s.name = _sensor_name(key,value);
                }
                if (key.rfind(SENSOR_OPTION_ADDRESS, 0) == 0) {
                    if (s.address = _sensor_address(key,value); s.address == -1) {
                        throw std::runtime_error("Syntax Error Sensor Adresse: " + std::to_string(s.address));
                    }
                }
                if (key.rfind(SENSOR_OPTION_DISABLED, 0) == 0) {
                    s.disabled = _sensor_disabled(key,value);
                }

                if (key.rfind(METRIC_OPTION_HELP, 0) == 0) {
                    metric.help = _metric_value(key,value);
                }
                if (key.rfind(METRIC_OPTION_TYPE, 0) == 0) {
                    metric.type = _metric_value(key,value);
                }
                if (key.rfind(METRIC_OPTION_NAME, 0) == 0) {
                    metric.name = _metric_value(key,value);
                }
                if (!metric.help.empty() && !metric.type.empty() && !metric.name.empty()) {
                    s.metric.emplace_back(metric);
                    metric = {};
                }
            }
            sensor.emplace_back(s);
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
std::string t_config::_trim_left(std::string& str) {
    const auto first_valid = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    str.erase(str.begin(), first_valid);
    return str;
}
// ----------------------------------------------------------------------------
std::string t_config::_trim_right(std::string& str) {
    const auto last_valid = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base(); // .base() wandelt den Reverse-Iterator in einen normalen Iterator um

    str.erase(last_valid, str.end());
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
bool t_config::_try_parse_int(const std::string& str, int& out)
{
    out = -1;
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
int t_config::_id(const std::string& key,const std::string& value)
{
    int a,b,c,id,s = 0;

    if (!value.empty() && _try_parse_int(value[0],a)) {
        s++;
        // pruefen ob die ID zweistellig ist
        if (value.length() == 2 && _try_parse_int(value[1],b)) {
            s++;
            // pruefen ob die ID dreistellig ist
            if (value.length() == 3 && _try_parse_int(value[2],c)) {
                s++;
            }
        }
    }

    switch (s) {
        case 1: {
            id = 1;
            break;
        }
        case 2: {
            id = (a * 10) + b;
            break;
        }
        case 3: {
            id = (a * 100) + (b * 10) + c;
        }
        default: {
            id = -1;
        }
    }
    return id;
}
// ----------------------------------------------------------------------------
std::string t_config::_sensor_name(const std::string& key,std::string& value)
{
    // wenn vorhanden, Anführungszeichen entfernen
    if (value.length() > 1) {
        if (value[0] == '"' && value[value.length() - 1] == '"') {
            value.replace(value.begin(), value.end(), "\"");
        }
        if (value[0] == '\'' && value[value.length() - 1] == '\'') {
            value.replace(value.begin(), value.end(), "'");
        }
    }
    return value;
}
// ----------------------------------------------------------------------------
int t_config::_sensor_address(const std::string& key,const std::string& value)
{
    if (int address; _try_parse_int(value,address)) {
        return address;
    }
    return -1;
}
// ----------------------------------------------------------------------------
bool t_config::_sensor_disabled(const std::string &key, const std::string &value)
{
    return (value == "true");
}
// ----------------------------------------------------------------------------
std::string t_config::_metric_value(const std::string& key,std::string& value)
{
    if (value.empty()) return "";

    // Prüfe und entferne am Ende (wichtig: zuerst Ende, um Index-Verschiebung zu vermeiden)
    char last = value.back();
    if (last == '"' || last == '\'') {
        value.pop_back();
    }

    // Prüfe und entferne am Anfang
    if (!value.empty()) {
        char first = value.front();
        if (first == '"' || first == '\'') {
            value.erase(0, 1);
        }
    }

    return value;
}
// ----------------------------------------------------------------------------
void t_config::_remove_signs(std::string& str)
{
    if (!str.empty()) {
        if (const char last = str.back(); last == '"' || last == '\'' || last == ']') {
            str.pop_back(); // am Ende entfernen
        }

        if (!str.empty()) {
            if (const char first = str.front(); first == '"' || first == '\'' || first == '[') {
                str.erase(0, 1);
            }
        }
    }
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------