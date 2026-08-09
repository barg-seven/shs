
#include "t_web_socket.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
// ----------------------------------------------------------------------------
t_web_socket::t_web_socket() = default;
// ----------------------------------------------------------------------------
t_web_socket::~t_web_socket() {

    std::lock_guard<std::mutex> lock(_mtx);

    for (std::thread& t : ct) {
        if (t.joinable()) {
            t.join();
        }
    }
};
// ----------------------------------------------------------------------------
int t_web_socket::send_frame(const int client, const std::string& json_text) {

    const size_t length = json_text.length();

    if (length > 65535) {
        return -1;
    }

    std::vector<uint8_t> frame;
    frame.reserve(4 + length);

    // 1. Byte: FIN + Text (0x81)
    frame.push_back(0x81);

    // 2. Byte & Längenfeld
    if (length <= 125) {
        frame.push_back(static_cast<uint8_t>(length));
    }
    else {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(length & 0xFF));
    }

    // KORREKTUR: Die JSON-Bytes direkt über die sichere Datenschnittstelle anfügen
    const auto* json_bytes = reinterpret_cast<const uint8_t*>(json_text.data());
    frame.insert(frame.end(), json_bytes, json_bytes + length);

    // Absenden
    return send(client, reinterpret_cast<const char*>(frame.data()), frame.size(), MSG_NOSIGNAL);
}
// ----------------------------------------------------------------------------
void t_web_socket::broadcast_relays_status(const std::string& json_text)
{
    // an alle verbundenen Browser senden
    for (auto it = _client_sockets.begin(); it != _client_sockets.end();) {
        if (const int fd = *it; send_frame(fd, json_text) < 0) {
            // wenn send fehlschlaegt, hat der User wohl den Tab geschlossen
            close(fd);
            it = _client_sockets.erase(it); // aus der Liste loeschen
        } else {
            ++it;
        }
    }
}
// ----------------------------------------------------------------------------
/**
 * @brief Extrahiert den Web-Socket-Schluessel des Clients.
 * @details Diese Methode extrahiert den Web-Socket-Schluessel des Clients, der
 * vom Browser erzeugt und gesendet wurde. Die Schluessel wird f. die Berechnung des
 * Web-Socket-Accept-Schluessels benoetigt.
 *
 * @param http_request Das ist der HTTP-Request-Header so wie er vom Browser des Clients kommt.
 * @return Gibt den Web-Socket-Schluessel des Clients zurueck.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
std::string t_web_socket::get_client_key(std::istringstream& http_request) {

    std::string line,key;

    // vor dem Lesen Fehlerbits und Positionszeiger zuruecksetzen
    http_request.clear();
    http_request.seekg(0);

    while (std::getline(http_request, line)) {

        // falls Windows-Zeilenumbrueche (\r\n) drin sind, das \r am Ende entfernen
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // nach dem Web-Socket-Schluessel suchen
        if (line.rfind("Sec-WebSocket-Key: ", 0) == 0) {
            key = line.substr(19); // schneidet "Sec-WebSocket-Key: " ab
            break;
        }
    }

    while (!key.empty() && (key.back() == '\r' || key.back() == '\n' || key.back() == ' ')) {
        key.pop_back();
    }

    return key;
}
// ----------------------------------------------------------------------------
/**
 * @brief Erzeugt einen Accept-Schluessel f. den Web-Socket.
 * @details Diese Methode erzeugt einen Web-Socket-Accept-Schluessel der f.
 * die Erstellung des Headers, welcher zurueck an den Client gesendet wird, benoetigt wird.
 *
 * @param client_key Das ist der Schluessel, den der Browser im Header "Sec-WebSocket-Key" sendet.
 * @return Gibt den fertig berechneten Accept-Schluessel zurueck.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
std::string t_web_socket::create_accept_key(const std::string& client_key) const {

    const std::string tmp = client_key + _ws_uuid;

    // den SHA-1 Hash berechnen
    const std::vector<uint8_t> hash = _sha1(tmp);

    // base64 kodieren
    return _base64(hash.data(),20);
}
// ----------------------------------------------------------------------------
/**
 * @brief Erzeugt die HTTP-Antwort f. den Client.
 * @details Diese Methode erzeugt den vollstaendigen HTTP-Antwort-Header f. den Protokoll-Wechsel (101 Switching Protocols).
 *
 * @param response Das ist fertige Header, der an den Client gesendet wird.
 * @param accept_key Ist der Web-Socket-Accept-Key. Der Key wird mit der Methode
 * create_accept() erzeugt.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
void t_web_socket::create_http_response_header(std::string& response,const std::string& accept_key) {

    response = "HTTP/1.1 101 Switching Protocols\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Accept: " + accept_key + "\r\n"
    "\r\n";

    // "Sec-WebSocket-Extensions:\r\n"
}
// ----------------------------------------------------------------------------
/**
 * @brief Ermittelt, ob es eine Anfrage f. einen Web-Socket ist.
 * @details Diese Methode ermittelt ob der HTTP-Request f. einen Web-Socket ist.
 *
 * @param http_request Ist der HTTP-Request vom Client.
 * @return Gibt bei Erfolg true ansonsten false zurueck.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
bool t_web_socket::web_socket_request(std::istringstream& http_request) {

    std::string line;
    bool upgrade_header_exists = false;
    bool connection_header_exists = false;

    // vor dem Lesen Fehlerbits und Positionszeiger zuruecksetzen
    http_request.clear();
    http_request.seekg(0);

    while (std::getline(http_request, line)) {

        // falls Windows-Zeilenumbrueche (\r\n) drin sind, das \r am Ende entfernen
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.find("Upgrade:") != std::string::npos && (line.find("websocket") != std::string::npos || line.find("WebSocket") != std::string::npos)) {
            upgrade_header_exists = true;
        }

        if (line.find("Connection:") != std::string::npos && line.find("Upgrade") != std::string::npos) {
            connection_header_exists = true;
        }
    }
    return (upgrade_header_exists && connection_header_exists);
}
// ----------------------------------------------------------------------------
/**
 * @brief Speichert einen Client im Client-Vector.
 * @details Diese Methode speichert einen Client in dem Client-Vector. Der Client-Vector
 * enthaelt alle Clients die ueber einen Web-Socket mit dem Server verbunden sind. Bei einer
 * Status-Aenderung von einem Relais, werden alle Clients informiert, die in dem Client-Vector stehen.
 *
 * @param client Der File Descriptor des TCP-Sockets.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
void t_web_socket::add_client(const int& client) {

    std::lock_guard<std::mutex> lock(_mtx);

    _client_sockets.push_back(client);
}
// ----------------------------------------------------------------------------
/**
 * @brief Loescht einen Client aus dem Client-Vector.
 * @details Siehe Methode add_client().
 *
 * @param client Der File Descriptor des TCP-Sockets.
 *
 * @author Andreas Jentsch
 * @date 27.06.2026
 */
void t_web_socket::remove_client(const int& client) {

    std::lock_guard<std::mutex> lock(_mtx);

    for (auto it = _client_sockets.begin(); it != _client_sockets.end();++it) {
        if (*it == client) {
            _client_sockets.erase(it);
            break;
        }
    }
}
// ----------------------------------------------------------------------------
std::string t_web_socket::receive_frame(const int client_fd, bool& verbindung_offen) {
    verbindung_offen = true;
    uint8_t header[2];

    // 1. Die ersten 2 Bytes des Headers lesen
    if (recv(client_fd, header, 2, 0) <= 0) {
        verbindung_offen = false;
        return "";
    }

    const uint8_t opcode = header[0] & 0x0F;
    const uint8_t mask_bit = (header[1] >> 7) & 0x01;
    uint64_t payload_len = header[1] & 0x7F;

    // Verbindung wurde vom Browser geschlossen (Opcode 0x08 ist Connection Close)
    if (opcode == 0x08) {
        verbindung_offen = false;
        return "";
    }

    // Wenn die Länge 126 oder 127 ist, folgten weitere Längen-Bytes (hier vereinfacht für kurze Befehle)
    if (payload_len == 126) {
        uint8_t ext_len[2];
        if (recv(client_fd, ext_len, 2, 0) <= 0) { verbindung_offen = false; return ""; }
        payload_len = (ext_len[0] << 8) | ext_len[1];
    }

    // Daten vom Browser MÜSSEN maskiert sein
    if (mask_bit == 0) {
        return ""; // Protokollfehler vom Browser, ignorieren
    }

    // 2. Die 4 Maskierungs-Bytes (Schlüssel) einlesen
    uint8_t masking_key[4];
    if (recv(client_fd, masking_key, 4, 0) <= 0) {
        verbindung_offen = false;
        return "";
    }

    // 3. Die maskierten Nutzdaten einlesen
    std::vector<uint8_t> payload(payload_len);
    size_t gesamt_gelesen = 0;
    while (gesamt_gelesen < payload_len) {
        ssize_t gelesen = recv(client_fd, payload.data() + gesamt_gelesen, payload_len - gesamt_gelesen, 0);
        if (gelesen <= 0) { verbindung_offen = false; return ""; }
        gesamt_gelesen += gelesen;
    }

    // 4. MATHEMATISCHE ENTMASKIERUNG (XOR-Verknüpfung)
    std::string entmaskierter_text = "";
    entmaskierter_text.reserve(payload_len);
    for (size_t i = 0; i < payload_len; ++i) {
        // Jedes Byte wird mit einem der 4 Schlüssel-Bytes XOR-verknüpft
        entmaskierter_text.push_back(static_cast<char>(payload[i] ^ masking_key[i % 4]));
    }

    return entmaskierter_text;
}
// ----------------------------------------------------------------------------
/**
 * @brief Bit-Rotation (Hilfsfunktion f. SHA1)
 * @param value S.
 * @param bits S.
 * @return S.
 *
 * @author KI
 * @date 28.06.2026
 */
uint32_t t_web_socket::_rol(const uint32_t value, const size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}
// ----------------------------------------------------------------------------
/**
 * @brief Berechnet eine SHA1-Hash.
 *
 * @param input Daten.
 * @return Gibt einen SHA1-Hash zurueck.
 *
 * @author KI
 * @date 28.06.2026
 */
std::vector<uint8_t> t_web_socket::_sha1(const std::string& input) {

    uint32_t digest[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    std::vector<uint8_t> pbuf(input.begin(), input.end());

    const uint64_t total_bits = pbuf.size() * 8;
    pbuf.push_back(0x80);
    while ((pbuf.size() + 8) % 64 != 0) pbuf.push_back(0x00);

    for (int i = 7; i >= 0; --i) pbuf.push_back((total_bits >> (i * 8)) & 0xFF);

    for (size_t chunk = 0; chunk < pbuf.size(); chunk += 64) {
        uint32_t w[80];
        for (size_t i = 0; i < 16; ++i) {
            w[i] = (pbuf[chunk + i * 4] << 24) | (pbuf[chunk + i * 4 + 1] << 16) |
                   (pbuf[chunk + i * 4 + 2] << 8) | pbuf[chunk + i * 4 + 3];
        }
        for (size_t i = 16; i < 80; ++i) {
            w[i] = _rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = digest[0], b = digest[1], c = digest[2], d = digest[3], e = digest[4];
        for (size_t i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }

            const uint32_t temp = _rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = _rol(b, 30); b = a; a = temp;
        }
        digest[0] += a; digest[1] += b; digest[2] += c; digest[3] += d; digest[4] += e;
    }

    std::vector<uint8_t> res(20);
    for (size_t i = 0; i < 5; ++i) {
        res[i * 4]     = (digest[i] >> 24) & 0xFF;
        res[i * 4 + 1] = (digest[i] >> 16) & 0xFF;
        res[i * 4 + 2] = (digest[i] >> 8) & 0xFF;
        res[i * 4 + 3] = digest[i] & 0xFF;
    }
    return res;
}
// ----------------------------------------------------------------------------
/**
 * @brief Kodiert eine Zeichenkette in Base64.
 *
 * @param input Das sind die Daten die codiert werden.
 * @param length Ist die Groesze der Eingabedaten.
 * @return Gibt den in Base64 kodierten String zurueck.
 *
 * @author KI
 * @date 28.06.2026
 */
std::string t_web_socket::_base64(const uint8_t* input, size_t length) {
    constexpr char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((length + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < length; i += 3) {
        result.push_back(lookup[(input[i] >> 2) & 0x3F]);
        result.push_back(lookup[((input[i] & 0x3) << 4) | ((input[i + 1] >> 4) & 0x3F)]);
        result.push_back(lookup[((input[i + 1] & 0xF) << 2) | ((input[i + 2] >> 6) & 0x3F)]);
        result.push_back(lookup[input[i + 2] & 0x3F]);
    }
    if (i < length) {
        result.push_back(lookup[(input[i] >> 2) & 0x3F]);
        if (i + 1 == length) {
            result.push_back(lookup[(input[i] & 0x3) << 4]);
            result.push_back('=');
        } else {
            result.push_back(lookup[((input[i] & 0x3) << 4) | ((input[i + 1] >> 4) & 0x3F)]);
            result.push_back(lookup[(input[i + 1] & 0xF) << 2]);
            result.push_back('=');
        }
    }
    return result;
}