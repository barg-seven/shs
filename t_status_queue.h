//
// Created by aj on 6/18/26.
//

#ifndef SHS_T_STATUS_QUEUE_H
#define SHS_T_STATUS_QUEUE_H

#include <mutex>
#include <string>
#include <sstream>
#include <vector>
#include "t_waveshare_card.h"

class t_status_queue {

public:

    std::vector<t_waveshare_card> cards;
    void set_state(int cardIndex,int outputIndex,uint8_t status);
    uint8_t get_state(int cardIndex, int outputIndex);
    void get_all_states_as_json(std::ostringstream& json);

private:
    std::mutex _mutex;
};


#endif //SHS_T_STATUS_QUEUE_H
