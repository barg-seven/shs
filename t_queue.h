

#ifndef SHS_ENHANCED_T_QUEUE_H
#define SHS_ENHANCED_T_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename t>
class t_queue
{
    public:
    void push(t val);
    t pop();
    bool pop_with_timeout(t& data,const int& timeout);

    private:
    std::queue<t> _q;
    std::mutex _m;
    std::condition_variable _cv;
};

// ----------------------------------------------------------------------------
/**
 * @brief Speichert Daten in der Queue.
 *
 * @details
 * Diese Methode speichert Daten in der Queue.
 *
 * @param val Enthaelt Daten zum Schreiben.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
template <typename t>
void t_queue<t>::push(t val)
{
    std::lock_guard<std::mutex> lock(_m);
    _q.push(val);
    _cv.notify_one();
}
// ----------------------------------------------------------------------------
/**
 * @brief Holt Daten aus der Queue.
 *
 * @details
 * Diese Methode blockiert einen Tread und holt die Daten aus der Queue. Blockiert bedeutet, der Tread
 * wartet so lange bis Daten mit push() in die Queue geschrieben wurden.
 *
 * @return Gibt die Daten aus der Queue zurueck.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
template <typename t>
t t_queue<t>::pop()
{
    std::unique_lock<std::mutex> lock(_m);
    _cv.wait(lock, [this] { return !_q.empty(); });
    t val = _q.front();
    _q.pop();
    return val;
}
// ----------------------------------------------------------------------------
/**
 * @brief Holt Daten aus der Queue.
 *
 * @details
 * Diese Methode blockiert einen Tread und holt Daten aus der Queue. Blockiert bedeutet, der Tread
 * wartet so lange bis Daten mit push() in die Queue geschrieben wurden oder
 * der Timeout abgelaufen ist.
 *
 * @param data Sind die Daten, die verarbeitet werden. Hier steht nur was drin,
 * wenn die Methode true zurückgibt.
 * @param timeout Enthaelt die Millisekunden die der Thread wartet.
 *
 * @return Wenn Daten in der Queue vorhanden sind wird true, ansonsten false
 * zurueckgegeben.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
template <typename t>
bool t_queue<t>::pop_with_timeout(t& data,const int& timeout)
{
    std::unique_lock<std::mutex> lock(_m);

    // wartet, bis Daten da sind oder die Zeit abgelaufen ist
    const bool rc = _cv.wait_for(lock, std::chrono::milliseconds(timeout), [this] {
        return !_q.empty();
    });

    if (rc) {
        data = _q.front(); // daten aus der Queue holen
        _q.pop(); // daten aus der Queue entfernen
        return true; // befehl erhalten
    }
    return false; // timeout abgelaufen, keine Daten da
}
// ----------------------------------------------------------------------------


#endif //SHS_ENHANCED_T_QUEUE_H



































// ###############################################################################################
/*
#ifndef SHS_T_THREAD_QUEUE_H
#define SHS_T_THREAD_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
*/
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

/*
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

class t_queue {

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
*/