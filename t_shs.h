//
// Created by aj on 5/28/26.
//

#ifndef SHS_T_APP_H
#define SHS_T_APP_H


#include <string>
#include <vector>



#include <sstream>
#include <map>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <termios.h>    // f. TCIOFLUSH

/**
 * @author Andreas Jentsch
 * @date 17.06.2026
 * @brief Der globale Speicher f. alle Relais-Zustaende
 *
 * @details
 * Ueber diesen globalen Speicher tauschen die Threads die Zustaende der
 * Ein- und Ausgaenge auf den Waveshare Relais-Karten aus. Auszerdem kann hier vom HTTP-Thread
 * ein neuer Zustand z. B. (Licht einschalten) gespeichert werden, der dann vom Thread mbus-h-outputs
 * abgearbeitet wird.
 */






class t_shs {
    public:
    t_shs() = default;
    ~t_shs() = default;

    static std::map<std::string,std::string> get_query_string(std::istringstream& http_request);
    static std::map<std::string,std::string> get_command(const std::string& json);
};






struct t_input {
    int index = 0;
    std::string type;
};
struct t_output {
    int index{};
    std::string type;
    int last_status{};
};
struct t_card {
    int index{};
    int address{};
    std::vector<t_input> input;
    std::vector<t_output> output;

    t_card() {
        input.resize(8);
        output.resize(8);
        for(int i = 0; i < 8; ++i) {
            input[i].index = i;
            output[i].index = i;
        }
    }
};



#endif //SHS_T_APP_H
