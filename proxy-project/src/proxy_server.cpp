#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <atomic>
#include "config.hpp"
#include "filter.hpp"
#include "logger.hpp"
#include "http_parser.hpp"

class ProxyServer {
private:
    int server_socket;
    std::atomic<bool> running;
    Config config;
    Filter filter;
    Logger logger;

public:
    // Load config
    ProxyServer(const std::string& config_file) 
        : server_socket(-1), running(true), config(config_file), 
          filter(config.filter_file), logger(config.log_file) {
    }

    ~ProxyServer() {
        shutdown_server();
    }

    bool initialize() {
        //create socket
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            std::cerr << "Error: Couldn't create socket." << std::endl;
            return false;
        }

        // allow port to reuse
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            std::cerr << "Error: Couldn't set socket options." << std::endl;
            return false;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(config.listen_address.c_str());
        server_addr.sin_port = htons(config.listen_port);

        // bind
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error: Couldn't bind to port " << config.listen_port << std::endl;
            return false;
        }

        //listen
        if (listen(server_socket, config.max_connections) < 0) {
            std::cerr << "Error: Couldn't listen on socket." << std::endl;
            return false;
        }

        logger.log("Proxy started on " + config.listen_address + ":" + std::to_string(config.listen_port));
        return true;
    }

    void run() {
        std::cout << "Proxy is running" << std::endl;

        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            // Wait for new client to connect
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket < 0) {
                if (running) logger.log("Warning: Failed to accept a connection.");
                continue;
            }


            std::string client_ip = inet_ntoa(client_addr.sin_addr);
            
            // thread for each client
            std::thread client_thread(&ProxyServer::handle_client, this, client_socket, client_ip);
            client_thread.detach(); 
        }
    }

    void shutdown_server() {
        running = false;
        if (server_socket >= 0) {
            close(server_socket);
            server_socket = -1;
        }
        logger.log("Proxy server has shut down.");
    }

private:
    //grab header
    std::pair<std::string, std::string> receive_headers_only(int client_socket) {
        std::string headers;
        char buf[8192];
        int header_end_pos = -1;

        //check for \r\n\r\n
        while (header_end_pos == -1) {
            ssize_t bytes = recv(client_socket, buf, sizeof(buf), 0);
            if (bytes <= 0) return {"", ""};

            headers.append(buf, bytes);
            
            header_end_pos = headers.find("\r\n\r\n");
            
            if (header_end_pos != std::string::npos) {
                std::string overflow = headers.substr(header_end_pos + 4);
                headers = headers.substr(0, header_end_pos + 4);
                return {headers, overflow};
            }
        }
        return {"", ""};
    }

    void handle_client(int client_socket, std::string client_ip) {
        struct timeval timeout;
        timeout.tv_sec = 3; 
        timeout.tv_usec = 0;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        // get header
        auto request_data = receive_headers_only(client_socket);
        std::string header_str = request_data.first;
        std::string overflow = request_data.second;

        if (header_str.empty()) {
            send_error_response(client_socket, 400, "Bad Request");
            close(client_socket);
            return;
        }

        // parse
        HttpRequest http_req;
        if (!http_req.parse(header_str)) {
            logger.log_request(client_ip, "UNKNOWN", "Invalid Request", "400 Bad Request");
            send_error_response(client_socket, 400, "Bad Request");
            close(client_socket);
            return;
        }

        // no HTTPS support
        if (http_req.method == "CONNECT") {
            logger.log_request(client_ip, http_req.method, http_req.host, "501 Not Implemented");
            send_error_response(client_socket, 501, "Not Implemented (HTTPS not supported)");
            close(client_socket);
            return;
        }

        // check if blocked
        if (filter.is_blocked(http_req.host)) {
            logger.log_request(client_ip, http_req.method, http_req.host, "403 BLOCKED");
            send_error_response(client_socket, 403, "Forbidden - Site Blocked");
            close(client_socket);
            return;
        }

        long long content_length = 0;
        size_t cl_pos = header_str.find("Content-Length:");
        if (cl_pos == std::string::npos) cl_pos = header_str.find("content-length:");
        
        if (cl_pos != std::string::npos) {
            size_t line_end = header_str.find("\r\n", cl_pos);
            std::string val = header_str.substr(cl_pos + 15, line_end - (cl_pos + 15));
            try { content_length = std::stoll(val); } catch(...) { content_length = 0; }
        }

        forward_request(client_socket, http_req, header_str, overflow, content_length, client_ip);
        
        close(client_socket);
    }

    void forward_request(int client_socket, HttpRequest& http_req, std::string& headers, 
                        std::string& overflow, long long content_length, std::string client_ip) {
        
        long long upload_bytes = 0;
        long long download_bytes = 0;


        struct addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        if (getaddrinfo(http_req.host.c_str(), std::to_string(http_req.port).c_str(), &hints, &res) != 0) {
            logger.log_request(client_ip, http_req.method, http_req.host, "502 DNS Error");
            send_error_response(client_socket, 502, "Bad Gateway - DNS Resolution Failed");
            return;
        }

        // connect to webserver
        int remote_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (connect(remote_socket, res->ai_addr, res->ai_addrlen) < 0) {
            close(remote_socket);
            freeaddrinfo(res);
            logger.log_request(client_ip, http_req.method, http_req.host, "502 Connection Failed");
            send_error_response(client_socket, 502, "Bad Gateway - Could not connect");
            return;
        }
        freeaddrinfo(res);

        // send headers
        ssize_t sent = send(remote_socket, headers.c_str(), headers.size(), 0);
        if (sent > 0) upload_bytes += sent;


        if (!overflow.empty()) {
            sent = send(remote_socket, overflow.c_str(), overflow.size(), 0);
            if (sent > 0) upload_bytes += sent;
        }

        // Stream the rest of the body (if any)
        long long bytes_remaining = content_length - overflow.size();
        char buf[8192];

        while (bytes_remaining > 0) {
            int to_read = (bytes_remaining > 8192) ? 8192 : bytes_remaining;
            int bytes = recv(client_socket, buf, to_read, 0);
            if (bytes <= 0) break;

            sent = send(remote_socket, buf, bytes, 0);
            if (sent > 0) upload_bytes += sent;
            
            bytes_remaining -= bytes;
        }


        
        int bytes;
        while ((bytes = recv(remote_socket, buf, 8192, 0)) > 0) {
            sent = send(client_socket, buf, bytes, 0);
            if (sent > 0) download_bytes += sent;
        }

        close(remote_socket);

        std::string stats = "200 OK (Up: " + std::to_string(upload_bytes) + 
                            "B, Down: " + std::to_string(download_bytes) + "B)";
        logger.log_request(client_ip, http_req.method, http_req.host, stats);
    }

    void send_error_response(int socket, int code, const std::string& message) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " + message + "\r\n";
        response += "Connection: close\r\n\r\n";
        send(socket, response.c_str(), response.length(), 0);
    }
};

ProxyServer* global_proxy = nullptr;

void signal_handler(int signum) {
    std::cout << "\nShutting down proxy server..." << std::endl;
    if (global_proxy) {
        global_proxy->shutdown_server();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }


    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ProxyServer proxy(argv[1]);
    global_proxy = &proxy;

    if (!proxy.initialize()) {
        std::cerr << "Failed to initialize proxy server" << std::endl;
        return 1;
    }

    proxy.run();
    return 0;
}