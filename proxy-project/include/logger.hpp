#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

class Logger {
private:
    std::ofstream log_file;
    std::mutex log_mutex;
    std::string filename;

public:
    Logger(const std::string& log_filename) : filename(log_filename) {
        log_file.open(log_filename, std::ios::app);
        if (!log_file.is_open()) {
            std::cerr << "Warning: Could not open log file " << log_filename << std::endl;
        }
    }

    ~Logger() {
        if (log_file.is_open()) {
            log_file.close();
        }
    }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        
        std::string timestamp = get_timestamp();
        std::string log_message = timestamp + " - " + message;

        // Write to file
        if (log_file.is_open()) {
            log_file << log_message << std::endl;
            log_file.flush();
        }

        // Also write to console
        std::cout << log_message << std::endl;
    }

    void log_request(const std::string& client_ip, const std::string& method, 
                     const std::string& url, const std::string& status) {
        std::stringstream ss;
        ss << client_ip << " - " << method << " " << url << " - " << status;
        log(ss.str());
    }

private:
    std::string get_timestamp() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

#endif // LOGGER_HPP