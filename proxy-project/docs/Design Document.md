
## High-Level Architecture

- The Proxy functions as an intermediary (Forward Proxy) between a client (e.g., `curl` or a web browser) and the internet. It operates at the Application Layer



## Component Descriptors 

### 1. Proxy Server  

- **Role:** It manages the lifecycle of the server socket, accepts incoming TCP connections, and spawns threads for client handling.

- **Key Responsibility:** socket binding, listening, and the main `accept()` loop.

### 2. Filter (Access Control):

- **Role:** Security gatekeeper. It maintains a list of blocked domains and IPs.

- **Key Feature:** Implements **Hot-Reloading**. It checks the modification timestamp of `filter.txt` on every request. If changed, it reloads the rules into memory using thread-safe mutex locks.

### 3. Http Request (Parser): 

-  **Role:** Lightweight protocol parser. It reads the raw byte stream from the socket and extracts the `Method` (GET/POST), `Host`, `Port`, and `Content-Length`.

- **Optimization:** It only parses the HTTP Headers. The Body is treated as a raw stream to avoid memory overhead.

### 4. Logger (Observability):

- **Role:** Thread-safe logging mechanism. Ensures that logs from multiple concurrent threads do not interleave or garble console output.


## 2. Concurrency Model

**Current Model: Thread-Per-Connection (Detached)**

### Description

For every new client connection accepted by the main loop, the system spawns a new `std::thread` and immediately detaches it (`client_thread.detach()`). This thread runs the `handle_client` function and terminates automatically when the socket closes.

### Rationale

-  This model avoids the complexity of managing a task queue or an event loop (like `epoll` or `boost::asio`).

-  If one client thread crashes or blocks on a slow DNS resolution, it does not freeze the main accept loop or other clients.
  


## 3. Data Flow (Streaming Architecture)

The defining feature of this proxy is its **Zero-Buffering Streaming** design.

### A. Incoming Request Handling

1. **Header :** The server reads from the client socket _only_ until it finds the **double CRLF (`\r\n\r\n`).**

2. **Overflow Capture:** Any bytes read beyond the headers (part of the body) are stored in a temporary `overflow` buffer.
   
3. **Parsing:** The headers are parsed to determine the target Host and `Content-Length`.

4. **Filtering:** The Host is checked against the `Filter` list. If blocked, a `403 Forbidden` is returned immediately.
   

### B. Outbound Forwarding (The "Pipe")

1. The proxy resolves the target IP and establishes a connection to the remote server.

2. **Header Relay:** The parsed headers are forwarded to the remote server.

3. **Body Streaming (Uploads):**

    - If there was `overflow` data, it is sent first.
    
    - The proxy enters a loop reading `8KB` chunks from the Client and writing them immediately to the Remote Server.
    
    - _Result:_ A 1GB upload never consumes more than ~8KB of RAM on the proxy.
    
4. **Response Streaming (Downloads):**

    - The proxy enters a loop reading `8KB` chunks from the Remote Server and writing them immediately to the Client.
       
	 - This continues until the remote server closes the connection or the Content-Length is reached.


## 4. Error Handling & Security

### Error Handling Strategy

The proxy handles errors gracefully by returning standard HTTP Status Codes to the client instead of crashing.

| **Event**            | **HTTP Code**         | **Description**                                 |
| -------------------- | --------------------- | ----------------------------------------------- |
| **Parsing Failure**  | `400 Bad Request`     | Client sent malformed HTTP headers.             |
| **Filter Match**     | `403 Forbidden`       | Target domain is in the blocklist.              |
| **HTTPS Request**    | `501 Not Implemented` | Client tried to use CONNECT method (TLS).       |
| **DNS/Connect Fail** | `502 Bad Gateway`     | Unable to resolve or reach the upstream server. |

### Security Considerations

1. **Thread Safety:**

    - **Filter List:** protected by `std::mutex` to prevent race conditions during hot-reloads.

    - **Logging:** protected by `std::mutex` to prevent output corruption.

2. **Resource Limits:** The `config` struct defines a `max_connections` limit (passed to `listen()`) to prevent basic SYN flood attacks from overwhelming the OS backlog.

### Current Limitations

- No **HTTPS CONNECT** support yet.
- **IPv4 Only:** Currently configured for `AF_INET` socket structures.