//
// Created by aj on 5/29/26.
//

#include "t_config.h"

/**
 * @brief Versucht eine Zeichenkette in eine Zahl zu konvertieren.
 *
 * @param str Eine Zeichenkette.
 * @param outValue Der Wert von str als Ganzzahl.
 *
 * @return Wenn erfolgreich konvertiert wurde, wird True ansonsten False zurueckgegeben.
 *
 * @author Andreas Jentsch
 * @date 24.06.2026
 */
bool t_config::try_parse_int(const std::string& str, int& outValue) {

    const char* start = str.data();
    const char* end = start + str.size();

    // versucht die Konvertierung
    auto [ptr, ec] = std::from_chars(start, end, outValue);

    // wenn kein Fehlercode (ec) vorliegt UND der gesamte String gelesen wurde,
    // war die Konvertierung zu 100% erfolgreich
    return (ec == std::errc() && ptr == end);
}
// ----------------------------------------------------------------------------
bool t_config::try_parse_int(const char ch, int& out_value) {

    return try_parse_int(std::string{ch}, out_value);
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
std::string t_config::trim(std::string& str) {
    const auto new_end = std::ranges::remove_if(str, [](const unsigned char ch) {
        return std::isspace(ch);
    }).begin();
    str.erase(new_end, str.end());

    return str;
}
// ----------------------------------------------------------------------------
/**
 * @brief Splittet eine Zeichenkette in Einzelteile.
 *
 * @details Splittet eine Zeichenkette, die Unterstriche als Trennzeichen enthaelt.
 * Die einzelnen Zeichenketten werden in einem std::vector<std::string> gespeichert. Jede dieser
 * einzelnen Zeichenketten stellt ein Element im std::vector<std::string> dar.
 *
 * @param text Eine Zeichenkette
 * @return std::vector<std::string>
 *
 * @author Andreas Jentsch
 * @date 28.05.2026
 */
std::vector<std::string> t_config::_split_by_underscore(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(text);

    // Liest den Stream bis zum Trennzeichen '_'
    while (std::getline(ss, token, '_')) {
        tokens.push_back(token);
    }

    return tokens;
}
// ----------------------------------------------------------------------------
void t_config::_init_options() {

        _file.open(_filename);

        if (!_file.is_open()) {
            std::cerr << "[Fehler] Konfigurationsdatei konnte nicht geöffnet werden!\n";
            return;
        }

        std::string line;
        int card_index = 0;

        // nach dem Pruefen auf eine Zahl, ist die Zahl hier gespeichert
        int number;

        // den Wert von card_x_address ermitteln und in einem Objekt von t_card speichern
        while (std::getline(_file, line)) {
            t_config::trim(line);

            // leere Zeilen und Kommentare ueberspringen
            if (line.empty() || line[0] == '#') continue;

            if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {

                // den Schluessel und Wert voneinander trennen
                std::string key = trim(line.substr(0, delimiter_pos));
                std::string value = trim(line.substr(delimiter_pos + 1));

                // nach card_x_address suchen
                if (key.rfind("card_") == 0 && key.length() > 4 && try_parse_int(key[5],number)) {
                    if (key.find("address") != std::string::npos && try_parse_int(value,number)) {

                        // ein Objekt von t_card erzeugen
                        t_card card;

                        card.index = card_index++;
                        card.address = number;
                        waveshare.card.push_back(card);
                    }
                }
            }
        }

        // fehlerbits loeschen und den Positionszeiger zuruecksetzen (fuer erneutes lesen der Konfig)
        _file.clear();
        _file.seekg(0, std::ios::beg);


        while (std::getline(_file, line)) {
            t_config::trim(line);

            // leere Zeilen und Kommentare ueberspringen
            if (line.empty() || line[0] == '#') continue;

            if (const size_t delimiter_pos = line.find('='); delimiter_pos != std::string::npos) {

                // den Schluessel und Wert voneinander trennen
                std::string key = trim(line.substr(0, delimiter_pos));
                std::string value = trim(line.substr(delimiter_pos + 1));

                //if (key.empty()) continue;

                // hier werden diese Optionen verarbeitet: card_0_input_0_type
                if (int tmp_i; key.rfind("card_", 0) == 0 && try_parse_int(key[5],tmp_i)) {
                    if (key.length() > 4) {
                        auto tmp_s = std::string(1,key[5]);
                        if (int card_n; try_parse_int(tmp_s,card_n)) {
                            tmp_s = std::string(1,key[13]);
                            if (int input_n; try_parse_int(tmp_s,input_n)) {

                                auto it = std::ranges::find_if(waveshare.card,[&](const t_card& c) {
                                    return c.index == card_n;
                                });

                                if (it != waveshare.card.end()) {
                                    it->input[input_n].type = value;
                                    it->input[input_n].index = input_n;
                                }
                            }
                        }
                    }
                    else {
                        options.insert(std::make_pair(key, value));
                    }
                }
                else {
                    options.insert(std::make_pair(key, value));
                }
            }
        }

    if (_file.is_open()) _file.close();

    };