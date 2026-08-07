//
// Created by aj on 5/31/26.
//

#include "t_thread_queue.h"

// ----------------------------------------------------------------------------
/**
 * @brief Speichert einen Befehl in der Thread-Queue.
 *
 * @details
 * Diese Methode speichert einen Befehl zum Schreiben in der Thread-Queue.
 *
 * @param data Enthaelt Daten zum Schreiben.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
void t_thread_queue::push(const t_command& data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _queue.push(data);

    // benachrichtigt den wartenden mbus-h-outputs, dasz neue Daten da sind
    _cv.notify_one();
}
// ----------------------------------------------------------------------------
/**
 * @brief Holt einen Befehl aus der Thread-Queue.
 *
 * @details
 * Diese Methode blockiert den Tread mbus-h-outputs und holt einen Befehl
 * zum Schreiben aus der Tread-Queue. Blockiert bedeutet, der Tread
 * wartet so lange bis Daten mit push() in die Queue geschrieben wurden.
 *
 * @param data Enthaelt Daten zum Schreiben.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
void t_thread_queue::pop(t_command& data)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] {
        return !_queue.empty();
    });
    data = _queue.front(); // daten aus der Queue holen
    _queue.pop(); // daten aus der Queue entfernen
}
// ----------------------------------------------------------------------------
/**
 * @brief Holt einen Befehl aus der Thread-Queue.
 *
 * @details
 * Diese Methode blockiert den Tread mbus-h-outputs und holt einen Befehl
 * zum Schreiben aus der Tread-Queue. Blockiert bedeutet, der Tread
 * wartet so lange bis Daten mit push() in die Queue geschrieben wurden oder
 * der Timeout abgelaufen ist.
 *
 * @param data Enthaelt Daten zum Schreiben.
 * @param timeout Enthaelt die Millisekunden die der Thread wartet.
 *
 * @return Wenn ein Befehl in der Thread-Queue steht wird true, ansonsten false
 * zurueckgegeben.
 *
 * @author Andreas Jentsch
 * @date 17.06.2026
 */
bool t_thread_queue::pop_with_timeout(t_command& data, const int timeout)
{
    std::unique_lock<std::mutex> lock(_mutex);

    // wartet, bis Daten da sind oder die Zeit abgelaufen ist
    const bool success = _cv.wait_for(lock, std::chrono::milliseconds(timeout), [this] {
        return !_queue.empty();
    });

    if (success) {
        data = _queue.front(); // daten aus der Queue holen
        _queue.pop(); // daten aus der Queue entfernen
        return true; // befehl erhalten
    }
    return false; // timeout abgelaufen, keine Daten da
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------