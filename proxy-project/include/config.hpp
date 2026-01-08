#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <map>

class Config {
public:
    std::string listen_address;
    int listen_port;
    int max_connections;
    std::string filter_file;
    std::string log_file;
    std::string cache_dir;
    bool enable_caching;

    Config(const std::string& config_file) 
        : listen_address("127.0.0.1"), 
          listen_port(8888), 
          max_connections(100),
          filter_file("filters.txt"),
          log_file("proxy.log"),
          cache_dir("cache"),
          enable_caching(false) {
        load(config_file);
    }

    void load(const std::string& config_file) {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open config file " << config_file 
                      << ", using defaults" << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Parse key=value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1));

            if (key == "listen_address") {
                listen_address = value;
            } else if (key == "listen_port") {
                listen_port = std::stoi(value);
            } else if (key == "max_connections") {
                max_connections = std::stoi(value);
            } else if (key == "filter_file") {
                filter_file = value;
            } else if (key == "log_file") {
                log_file = value;
            } else if (key == "cache_dir") {
                cache_dir = value;
            } else if (key == "enable_caching") {
                enable_caching = (value == "true" || value == "1");
            }
        }

        file.close();
    }

private:
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
};

#endif // CONFIG_HPP