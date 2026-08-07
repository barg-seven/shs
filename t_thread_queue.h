//
// Created by aj on 5/31/26.
//

#ifndef SHS_T_THREAD_QUEUE_H
#define SHS_T_THREAD_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

/**
 * @brief Diese Daten werden in die Thread-Queue geschrieben
 *
 * @details
 * Das sind die Daten, die zum Schalten eines Relais gebraucht werden. Sie werden ueber
 * die Thread-Queue an die Threads mbus-h-inputs und mbus-h-outputs gegeben.
 * - nb             = number bits, wieviele Bits sollen geschrieben werden
 * - dim            = zeigt ob gedimmt wird oder nicht
 * - card_index     = der Index der Relais-Karte
 * - card_address   = die Modbus Adresse der Karte (nicht der Index)
 * - output_index   = der Index eines Relais auf der Karte
 * - output_address = die Adresse des Relais 0 - 7
 * - status         = enthaelt den Wert, der geschrieben wird 0 oder 1
 * - action         = 0 = switch-eltako, 1 = dim-eltako-start, 2 = dim-eltako-stop, 3 = switch-relay
 *
 * @author Andreas Jentsch
 * @date 31.05.2026
 */
struct t_command {
    //int nb;
    bool dim;
    int card_index;
    int card_address;
    int output_index;
    int output_address;
    uint8_t status;
    bool impuls;
    bool no_impuls;
    int action;
};

class t_thread_queue {

public:
    void push(const t_command& data);
    void pop(t_command& data);
    bool pop_with_timeout(t_command& data, int timeout);

private:
    std::queue<t_command> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
};


#endif //SHS_T_THREAD_QUEUE_H
