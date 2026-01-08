#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <string>
#include <sstream>
#include <map>

class HttpRequest {
public:
    std::string method;
    std::string url;
    std::string version;
    std::string host;
    std::string path;
    int port;
    std::map<std::string, std::string> headers;
    std::string body;

    HttpRequest() : port(80) {}

    bool parse(const std::string& request) {
        std::istringstream stream(request);
        std::string line;

        if (!std::getline(stream, line)) return false;
        
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream request_line(line);
        if (!(request_line >> method >> url >> version)) {
            return false;
        }
        
        while (std::getline(stream, line) && line != "\r" && !line.empty()) {
            if (line.back() == '\r') line.pop_back();
            
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string header_name = line.substr(0, colon_pos);
                std::string header_value = line.substr(colon_pos + 1);
                
                size_t start = header_value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    header_value = header_value.substr(start);
                }
                
                headers[header_name] = header_value;
                
                if (header_name == "Host") {
                    parse_host(header_value);
                }
            }
        }

            // For other methods, parse URL
            parse_url();
        

        // Read body if present
        std::string remaining;
        while (std::getline(stream, line)) {
            body += line + "\n";
        }

        return !method.empty() && !host.empty();
    }

private:
    void parse_host(const std::string& host_header) {
        size_t colon_pos = host_header.find(':');
        if (colon_pos != std::string::npos) {
            host = host_header.substr(0, colon_pos);
            port = std::stoi(host_header.substr(colon_pos + 1));
        } else {
            host = host_header;
            port = 80; // Default HTTP port
        }
    }

    void parse_url() {
        // Check if URL is absolute (http://host/path)
        if (url.substr(0, 7) == "http://") {
            size_t host_start = 7;
            size_t path_start = url.find('/', host_start);
            
            if (path_start != std::string::npos) {
                std::string host_part = url.substr(host_start, path_start - host_start);
                path = url.substr(path_start);
                
                // Parse host and port from URL if Host header not present
                if (host.empty()) {
                    parse_host(host_part);
                }
            } else {
                // No path, just host
                std::string host_part = url.substr(host_start);
                path = "/";
                if (host.empty()) {
                    parse_host(host_part);
                }
            }
        } else {
            // Relative URL
            path = url;
        }
    }


};

#endif // HTTP_PARSER_HPP