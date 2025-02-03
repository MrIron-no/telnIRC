#ifndef CONFIG_H
#define CONFIG_H

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

#endif // CONFIG_H
