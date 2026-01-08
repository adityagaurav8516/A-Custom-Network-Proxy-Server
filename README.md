#  C++ HTTP Proxy Server

A lightweight, multi-threaded HTTP forward proxy server written in C++11. It handles concurrent connections, supports domain blacklisting (with hot-reloading), and streams data efficiently without high memory usage.



##  Features

* **Multi-threaded Architecture:** Handles multiple clients simultaneously using standard C++ threads.
* **HTTP Parsing:** Custom-built HTTP parser (supports GET, POST, HEAD, etc.).
* **Domain Filtering:** Blocks blacklisted domains returning `403 Forbidden`.
* **Hot-Reloading:** Update the blacklist (`filters.txt`) without restarting the server.
* **Data Streaming:** Efficiently pipes large uploads/downloads (e.g., 50MB files) with minimal RAM usage.
* **Detailed Logging:** Tracks Client IP, Method, URL, Status Code, and Bandwidth (Upload/Download bytes).
* **Robustness:** Includes socket timeouts and signal handling for graceful shutdowns.
* **Configurable:** Port, log file location, and connection limits are customizable.

##  Project Structure

```text
proxy-project/
├── src/
│   └── proxy_server.cpp       # Main server 
├── include/
│   ├── config.hpp             # Configuration loader
│   ├── filter.hpp             # Domain blocking 
│   ├── http_parser.hpp        # HTTP Request parsing
│   └── logger.hpp             # Thread-safe logging
├── config/
│   ├── proxy.conf             # Server settings
│   └── filters.txt            # Blacklisted domains
├── tests/
│   ├── tests.sh               # Automated regression tests
│   └── testing.txt            # Instructions fro testing
├── Makefile                   # Build automation
└── README.md                  # Documentation

