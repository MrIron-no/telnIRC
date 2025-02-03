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

#include <sys/stat.h>

#include "config.h"

bool ConfigParser::isValidFile(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);  // File exists
}

bool ConfigParser::load(const std::string& filename, const std::string& section) {
    if (!isValidFile(filename)) {
        std::cerr << "Error: Config file does not exist or cannot be accessed: " << filename << std::endl;
        ::exit(1);
        return false;
    }

    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Could not open config file: " << filename << std::endl;
        return false;
    }

    std::string line;
    bool in_section = false;

    while (std::getline(file, line)) {
        trim(line);

        // Ignore empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        // Check if it's a section
        if (line[0] == '[' && line.back() == ']') {
            in_section = (line.substr(1, line.size() - 2) == section);
            continue;
        }

        // If in the correct section, parse key-value
        if (in_section) {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                trim(key);
                trim(value); // Value can be empty
                config_map[key] = value; // Store empty values correctly
            }
        }
    }

    return true;
}

void ConfigParser::trim(std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    size_t last = str.find_last_not_of(" \t");
    str = (first == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

// Explicit specialization for std::string (handles empty values correctly)
template <>
std::string ConfigParser::convert<std::string>(const std::string& value, const std::string&) {
    return value;  // Empty values are valid
}

// Explicit specialization for bool (supports "on", "yes", "off", "no")
template <>
bool ConfigParser::convert<bool>(const std::string& value, const bool& default_value) {
    std::string v = value;
    for (auto& c : v) c = tolower(c); // Convert to lowercase

    if (v == "true" || v == "1" || v == "on" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "off" || v == "no") return false;
    return default_value;
}
