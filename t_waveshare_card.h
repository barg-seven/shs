//
// Created by aj on 6/18/26.
//

#ifndef SHS_T_WAVESHARE_CARD_H
#define SHS_T_WAVESHARE_CARD_H

#include <string>
#include <vector>


class t_waveshare_card {

    struct t_input {
        int index;
        int address;
        std::string type;
    };

    struct t_output {
        int index;
        int address;
        int status;
        int last_status;
    };

public:

    t_waveshare_card(const int& input_count, const int& output_count);
    ~t_waveshare_card();

    [[nodiscard]] inline int get_input_count() const {
        return _input_count;
    };
    [[nodiscard]] inline int get_output_count() const {
        return _output_count;
    };

    int index;
    int address;
    std::vector<t_input> inputs;
    std::vector<t_output> outputs;

private:
    int _input_count;
    int _output_count;
};


#endif //SHS_T_WAVESHARE_CARD_H
