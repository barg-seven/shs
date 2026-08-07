//
// Created by aj on 5/28/26.
//

#include "t_shs.h"

std::map<std::string,std::string> t_shs::get_query_string(std::istringstream& http_request) {

    std::string line;
    std::map<std::string, std::string> result;

    while (std::getline(http_request, line)) {

        // loescht \r am Ende
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("GET ", 0) == 0) {

            if (const size_t question_mark_pos = line.find('?'); question_mark_pos != std::string::npos) {

                const size_t space_pos = line.find(' ', question_mark_pos);

                // der Teil nach dem Fragezeichen z. B. card=0&relay=0&state=1
                std::string query_string = line.substr(question_mark_pos + 1, space_pos - question_mark_pos - 1);

                // den GET Parameter-String in Schluessel-Werte-Paare teilen, card=0&relay=0&state=1
                std::stringstream ss(query_string);
                std::string pair;

                // den String bei jedem '&' teilen
                while (std::getline(ss, pair, '&')) {
                    std::stringstream pairStream(pair);
                    std::string value;

                    // das Paar bei '=' in Key und Value aufteilen
                    if (std::string key; std::getline(pairStream, key, '=') && std::getline(pairStream, value)) {
                        auto start = std::ranges::find_if_not(value, [](const unsigned char ch) {
                            return std::isspace(ch) || ch == '\r' || ch == '\n';
                        });
                        auto end = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char ch) {
                            return std::isspace(ch) || ch == '\r' || ch == '\n';
                        }).base();

                        value =  (start < end) ? std::string(start, end) : "";
                        result[key] = value;
                    }
                }
            }
        }
    }

    return result;
}

std::map<std::string, std::string> t_shs::get_command(const std::string& json) {
    std::map<std::string, std::string> result;

    // Die drei Schlüssel, nach denen wir suchen
    std::vector<std::string> keys = {"action","card", "relay", "state"};

    for (const auto& key : keys) {
        // 1. Suche nach dem Schlüssel im JSON (z.B. "card")
        size_t keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos) continue; // Schlüssel nicht gefunden -> überspringen

        // 2. Suche den dazugehörigen Doppelpunkt nach dem Schlüssel
        size_t colonPos = json.find(":", keyPos);
        if (colonPos == std::string::npos) continue;

        // 3. Finde den Start des Wertes (überspringe den Doppelpunkt und eventuelle Leerzeichen)
        size_t valStart = json.find_first_not_of(" \t", colonPos + 1);
        if (valStart == std::string::npos) continue;

        // 4. Finde das Ende des Wertes (entweder am Komma oder an der schliessenden Klammer '}')
        size_t valEnd = json.find_first_of(",}", valStart);
        if (valEnd == std::string::npos) continue;

        // 5. Wert ausschneiden und in die Map einfügen
        std::string val = json.substr(valStart, valEnd - valStart);
        result[key] = val;
    }

    return result;
}


