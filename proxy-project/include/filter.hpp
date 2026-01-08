#ifndef FILTER_HPP
#define FILTER_HPP

#include <string>
#include <unordered_set>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

class Filter {
private:
std::string filename;
    std::unordered_set<std::string> blocked_domains;
    std::mutex mtx;          // Safety lock for threads
    time_t last_update_time; // Stores when we last read the file

    // Helper: Get the actual file modification timestamp from OS
    time_t get_file_timestamp() {
        struct stat result;
        if (stat(filename.c_str(), &result) == 0) {
            return result.st_mtime;
        }
        return 0;
    }


public:
    Filter(const std::string& file) : filename(file), last_update_time(0) {
        reload_list();
    }

    void reload_list() {
        std::ifstream file(filename);
        if (!file.is_open()) return;

        // Lock the door so no threads enter while we wipe the list
        std::lock_guard<std::mutex> lock(mtx); 
        
        blocked_domains.clear();
        std::string line;

        while (std::getline(file, line)) {
            // Simple trim (remove spaces)
            while (!line.empty() && isspace(line.back())) line.pop_back();
            while (!line.empty() && isspace(line.front())) line.erase(0, 1);

            if (!line.empty() && line[0] != '#') {
                blocked_domains.insert(line);
            }
        }
        
        // Update our timestamp so we know we are current
        last_update_time = get_file_timestamp();
        std::cout << "Filter list updated! " << blocked_domains.size() << " blocked items." << std::endl;
    }

    bool is_blocked(const std::string& host) {
        // 1. Check if file has changed since last time
        time_t current_time = get_file_timestamp();
        if (current_time > last_update_time) {
            reload_list();
        }

        // 2. Check the list (Thread safe)
        std::lock_guard<std::mutex> lock(mtx);

        // Exact match?
        if (blocked_domains.count(host)) return true;

        // Subdomain match? (e.g., "ads.google.com" matches "google.com")
        for (const auto& domain : blocked_domains) {
            if (host.length() > domain.length()) {
                // Check if host ends with domain
                if (host.substr(host.length() - domain.length()) == domain) {
                    return true;
                }
            }
        }

        return false;
    }
};

#endif