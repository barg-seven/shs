//
// Created by aj on 6/18/26.
//

#include "t_waveshare_card.h"

t_waveshare_card::t_waveshare_card(const int& input_count,const int& output_count) : index(0), address(0) {

    _input_count = input_count;
    _output_count = output_count;

    inputs.resize(_input_count);
    outputs.resize(_output_count);
}

t_waveshare_card::~t_waveshare_card() = default;