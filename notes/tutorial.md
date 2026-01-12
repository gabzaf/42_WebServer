# Webserv Development Tutorial: Foundation Phase

## 1. The Strategy: "Container vs. Content"
To manage the complexity of Webserv, we split development into two distinct streams:
1.  **The Container (Dev A)**: Infrastructure. This code manages *how* bytes travel (Sockets, Event Loop, Configuration).
2.  **The Content (Dev B)**: Logic. This code manages *what* the bytes mean (HTTP Parsing, Response Generation).

**Current Status**: We have completed the foundational work for **Dev A**.

---

## 2. Infrastructure Walkthrough

### The File Structure
- `includes/`: Header files (.hpp).
- `srcs/`: Implementation files (.cpp).
- `config/`: Configuration files (.conf).
- `Makefile`: Compiles the project with `-std=c++98`.

### The Config Parser (`srcs/Config.cpp`)
The parser converts a text file into usable C++ objects.
1.  **Structures** (`Config.hpp`):
    - `ServerConfig`: Holds global server settings (port, host, root, error_pages).
    - `LocationConfig`: Holds route-specific settings (autoindex, allowed methods, cgi).
2.  **Logic**:
    - `parse()`: Opens the file and reads line-by-line. It looks for `server {`.
    - `parseServer()`: When a server block is found, this function takes over. It parses directives like `listen` or `client_body_size` until it hits a closing `}`.
    - `parseLocation()`: If `location` is found inside a server, it recurses further to parse directives like `root` or `return`.
    - **Utils**: `trim()` removes whitespace, and `removeSemicolon()` ensures clean data.

### The Server Core (`srcs/Server.cpp`)
This is the engine of the application.
1.  **Initialization** (`initSocket`):
    - `socket()`: Creates the endpoint.
    - `setsockopt(SO_REUSEADDR)`: Critical for restarting the server instantly without "Port in use" errors.
    - `bind()`: Attaches the socket to the port (e.g., 8080).
    - `listen()`: Tells the OS to accept incoming connections.
    - `fcntl(O_NONBLOCK)`: **Crucial**. Ensures the server never freezes waiting for a client.

2.  **The Event Loop** (`run`):
    - We use `poll()`, which monitors a list of file descriptors (`_fds`).
    - The loop runs forever (`while(true)`), calling `poll()` to wait for activity.
    - **Activity Type 1 (Server Socket)**: If the main listener has activity, a new client is knocking. We call `acceptConnection()` to let them in and add them to our `poll` list.
    - **Activity Type 2 (Client Socket)**: If an existing client has activity, they sent data. We call `handleClient()` to read it.

---

## 3. Detailed Code Analysis

### `main.cpp`
```cpp
int main(int ac, char **av) {
    // 1. Determine config file path (default or argument)
    std::string config_file = (ac > 1) ? av[1] : "config/default.conf";
    
    try {
        // 2. Parse Config
        Config config(config_file);
        
        // 3. Initialize Server with the first parsed configuration
        Server server(config.getServers()[0]);
        
        // 4. Start the Loop
        server.run();
    } catch (...) {
        // Global error handling prevents crashes
    }
}
```

### `Server::handleClient` (Current Implementation)
Currently, this is a placeholder for Dev B's work.
```cpp
void Server::handleClient(int fd) {
    char buffer[1024];
    // 1. Read raw bytes
    ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0) {
        // Connection closed or error
        close(fd);
    } else {
        // 2. Logic Stub (Where Middleware/Dev B code will go)
        std::cout << "Received: " << buffer << std::endl;
        
        // 3. Send hardcoded response
        send(fd, "HTTP/1.1 200 OK...", ...);
    }
}
```

## 4. What's Next? (Dev B Handover)
The infrastructure is ready. The `handleClient` function receives raw bytes but doesn't know what they mean.
**Next Steps:**
1.  Create `HttpRequest` class to parse the raw buffer into:
    - Method (`GET`, `POST`)
    - Path (`/index.html`)
    - Headers (`Host: localhost`)
    - Body
2.  Create `HttpResponse` class to build the response string dynamically instead of hardcoding it.
