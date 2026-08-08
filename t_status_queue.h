
#ifndef SHS_T_STATUS_QUEUE_H
#define SHS_T_STATUS_QUEUE_H

#include <mutex>
#include <sstream>
#include <vector>
#include "t_waveshare_card.h"

typedef std::vector<t_waveshare_card> t_sq;

class t_status_queue {

public:

    t_status_queue();
    ~t_status_queue();

    std::mutex _mutex;
    std::vector<t_waveshare_card> cards;
    void set_state(int cardIndex,int outputIndex,uint8_t status);
    uint8_t get_state(int cardIndex, int outputIndex);
    void as_json(std::ostringstream& json);
};


#endif //SHS_T_STATUS_QUEUE_H
