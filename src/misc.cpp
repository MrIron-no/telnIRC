/**
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of

 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include <string>
#include <ctime>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <random>
#include <pwd.h>       // For getpwuid() and struct passwd
#include <sys/types.h> // For uid_t
#include <unistd.h>    // For getuid()
#include <cwchar>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "misc.h"

void strip_ircv3_message_tags(std::string& line) {
    if (line.empty() || line[0] != '@')
        return;
    // Tag keys/values cannot contain raw spaces (they use \s); first ASCII space ends the tag block.
    size_t sp = line.find(' ');
    if (sp == std::string::npos)
        return;
    line.erase(0, sp + 1);
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
        ++i;
    if (i > 0)
        line.erase(0, i);
}

static std::string base64_encode(const unsigned char* data, size_t len) {
    int out_len = 4 * static_cast<int>((len + 2) / 3);
    std::string out(out_len, '\0');
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()), data, static_cast<int>(len));
    out.resize(static_cast<size_t>(written));
    return out;
}

bool parse_host(const std::string& host, unsigned int default_port, HostConfig& out) {
    out = HostConfig{};
    out.original = host;

    std::string rest = host;
    if (rest.rfind("wss://", 0) == 0) {
        out.transport = HostConfig::Transport::WebSocket;
        out.implicit_tls = true;
        rest = rest.substr(6);
    } else if (rest.rfind("ws://", 0) == 0) {
        out.transport = HostConfig::Transport::WebSocket;
        out.implicit_tls = false;
        rest = rest.substr(5);
    }

    size_t slash = rest.find('/');
    std::string hostport = (slash != std::string::npos) ? rest.substr(0, slash) : rest;
    if (slash != std::string::npos) {
        out.path = rest.substr(slash);
        if (out.path.empty())
            out.path = "/";
    }

    size_t colon = hostport.rfind(':');
    if (colon != std::string::npos) {
        out.hostname = hostport.substr(0, colon);
        try {
            out.port = static_cast<unsigned int>(std::stoul(hostport.substr(colon + 1)));
        } catch (...) {
            return false;
        }
    } else {
        out.hostname = hostport;
        if (out.transport == HostConfig::Transport::WebSocket)
            out.port = out.implicit_tls ? 443 : 80;
        else
            out.port = default_port;
    }

    return !out.hostname.empty() && out.port > 0;
}

std::string sha1_base64(const std::string& input) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    EVP_Digest(input.data(), input.size(), digest, &digest_len, EVP_sha1(), nullptr);
    return base64_encode(digest, digest_len);
}

std::string generate_websocket_key() {
    unsigned char key[16];
    RAND_bytes(key, sizeof(key));
    return base64_encode(key, sizeof(key));
}

void utf8_pop_back(std::string& out) {
    if (out.empty())
        return;
    auto it = out.end();
    --it;
    while (it != out.begin() && (static_cast<unsigned char>(*it) & 0xC0) == 0x80)
        --it;
    out.erase(it, out.end());
}

int utf8_display_width(const std::string& text) {
    mbstate_t ps{};
    const char* src = text.c_str();
    size_t left = text.size();
    int width = 0;

    while (left > 0) {
        wchar_t wc;
        size_t n = mbrtowc(&wc, src, left, &ps);
        if (n == 0)
            break;
        if (n == static_cast<size_t>(-1) || n == static_cast<size_t>(-2))
            return width + static_cast<int>(left);
        int cw = wcwidth(wc);
        width += (cw >= 0) ? cw : 1;
        src += n;
        left -= n;
    }

    return width;
}

std::string get_timestamp() {
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm->tm_hour << ':'
        << std::setw(2) << tm->tm_min << ':'
        << std::setw(2) << tm->tm_sec;
    return oss.str();
}

std::string generate_random_number_string(size_t length) {
    static const char digits[] = "0123456789";
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0, sizeof(digits) - 2);

    std::string result;
    for (size_t i = 0; i < length; ++i) {
        result += digits[dist(generator)];
    }
    return result;
}

Params Tokenizer( const std::string& line ) {
    std::istringstream stream(line);
    std::vector<std::string> params;
    std::string word;

    // Read each space-separated word into the vector
    while (stream >> word) {
        params.push_back(word);
    }
    return params;
}

std::string get_unix_username() {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return std::string(pw->pw_name);
    } else {
        return "unknown";
    }
}