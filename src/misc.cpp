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
#include <sstream>
#include <iomanip>
#include <random>
#include <pwd.h>       // For getpwuid() and struct passwd
#include <sys/types.h> // For uid_t
#include <unistd.h>    // For getuid()

#include "misc.h"

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