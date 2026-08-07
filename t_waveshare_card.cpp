//
// Created by aj on 6/18/26.
//

#include "t_waveshare_card.h"

t_waveshare_card::t_waveshare_card(const int& inputCount,const int& outputCount) : index(0), address(0) {

    _input_count = inputCount;
    _output_count = outputCount;

    inputs.resize(_input_count);
    outputs.resize(_output_count);
}

t_waveshare_card::~t_waveshare_card() = default;