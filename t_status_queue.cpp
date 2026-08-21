
#include "t_status_queue.h"
#include <fstream>

// ----------------------------------------------------------------------------
t_status_queue::t_status_queue() = default;
// ----------------------------------------------------------------------------
t_status_queue::~t_status_queue() = default;
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void t_status_queue::set_state(const int cardIndex, const int outputIndex, const uint8_t status) {

    std::lock_guard<std::mutex> lock(_mutex);
    cards[cardIndex].outputs[outputIndex].status = status;
}
// ----------------------------------------------------------------------------
uint8_t t_status_queue::get_state(const int cardIndex, const int outputIndex) {

    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto & it : cards) {
        if (it.index == cardIndex) {
            return it.outputs[outputIndex].status;
        }
    }
    return -1; // standardwert, falls noch nicht gelesen
}
// ----------------------------------------------------------------------------
std::string t_status_queue::as_json()
{
    std::ostringstream json;
    as_json(json);
    return json.str();
}
// ----------------------------------------------------------------------------
std::string t_status_queue::as_json(std::ostringstream& json) {

    std::lock_guard<std::mutex> lock(_mutex);

    json.str("");
    json.clear();

    json << R"({"status": "success", "cards": [)";

    bool first_card = true;
    for (const auto & card : cards) {

        // komma vor der naechsten Karte setzen (ab der zweiten Karte)
        if (!first_card) {
            json << ",";
        }
        first_card = false;

        json << R"({"index": )" << card.index << R"(, "relays": [)";

        bool first_relay = true;
        for (const auto & relay : card.outputs) {

            // komma vor dem naechsten Relais setzen (ab dem zweiten Relais)
            if (!first_relay) {
                json << ",";
            }
            first_relay = false;

            json << R"({"index": )" << relay.index
                 << R"(,"status": )" << std::boolalpha << static_cast<bool>(relay.status)
                 << "}";
        }

        json << "]}"; // schliesst das "relays"-Array und das Karten-Objekt
    }

    json << "]}"; // schliesst das "cards"-Array und das Haupt-JSON

    return json.str();
}
// ----------------------------------------------------------------------------
void t_status_queue::safe_to_file(const std::string& json)
{
    if (std::ofstream f("/var/shs/sq.json"); f.is_open()) {
        f << json;
        f.close();
    }
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
