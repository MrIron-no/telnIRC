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

#pragma once

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <sstream>

class ConfigParser {
public:
    bool isValidFile(const std::string&);
    bool load(const std::string&, const std::string&);

    template <typename T>
    T get(const std::string& key, const T& default_value) const {
        auto it = config_map.find(key);
        if (it == config_map.end()) return default_value;
        return convert<T>(it->second, default_value);
    }

private:
    std::unordered_map<std::string, std::string> config_map;

    static void trim(std::string& str);

    // Convert template function (Generic case)
    template <typename T>
    static T convert(const std::string& value, const T& default_value) {
        std::istringstream iss(value);
        T result;
        if (!(iss >> result)) return default_value;
        return result;
    }
};

// Declare explicit specializations (definitions in config.cpp)
template <>
std::string ConfigParser::convert<std::string>(const std::string& value, const std::string& default_value);

template <>
bool ConfigParser::convert<bool>(const std::string& value, const bool& default_value);
