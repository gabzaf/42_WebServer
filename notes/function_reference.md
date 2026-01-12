# System Function Reference for Webserv (C++ 98)

This document provides examples and context for every allowed system function in the Webserv project. The goal is to build a non-blocking HTTP server.

## 1. Socket Setup & Connection Handling

These functions are used to initialize the server and accept incoming connections.

### `socket`
Creates an endpoint for communication.
```cpp
#include <sys/socket.h>

// domain: AF_INET (IPv4), type: SOCK_STREAM (TCP), protocol: 0 (default)
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd == -1) {
    perror("socket failed");
}
```

### `setsockopt`
Configures socket options. Crucial for `SO_REUSEADDR` to restart the server immediately on the same port.
```cpp
int opt = 1;
// level: SOL_SOCKET, option_name: SO_REUSEADDR
if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt failed");
}
```

### `htons`, `htonl`, `ntohs`, `ntohl`
Convert values between host (your computer) and network (Big Endian) byte order.
- `h` host, `n` network, `s` short (16-bit, ports), `l` long (32-bit, IP addresses).
```cpp
#include <netinet/in.h>

struct sockaddr_in address;
address.sin_family = AF_INET;
address.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces (0.0.0.0)
address.sin_port = htons(8080);       // Port 8080 converted to network byte order
```

### `bind`
Assigns the address (IP and Port) to the socket.
```cpp
if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
}
```

### `listen`
Marks the socket as passive, ready to accept connections.
```cpp
// backlog: maximum length of the queue of pending connections
if (listen(server_fd, 10) < 0) {
    perror("listen failed");
}
```

### `accept`
Extracts the first connection request from the queue, creates a *new* socket for that client.
```cpp
int client_fd;
struct sockaddr_in client_addr;
socklen_t client_len = sizeof(client_addr);

// This call blocks by default! Webserv requires non-blocking usage via poll/select.
client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
```

### `fcntl`
Manipulates file descriptor flags. **Critical for making sockets non-blocking.**
```cpp
#include <fcntl.h>

// Get current flags
int flags = fcntl(server_fd, F_GETFL, 0);
// Set O_NONBLOCK flag
fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
```

---

## 2. I/O Multiplexing (The Event Loop)

The subject requires using **one** `poll` (or equivalent) to manage all IO. This allows the server to handle multiple clients simultaneously without threads.

### `poll` (Recommended for portability/simplicity)
```cpp
#include <poll.h>
#include <vector>

std::vector<struct pollfd> fds;

// Add server socket to monitor
struct pollfd server_pollfd;
server_pollfd.fd = server_fd;
server_pollfd.events = POLLIN; // Monitor for incoming data (new connections)
fds.push_back(server_pollfd);

// Main Loop
while (true) {
    int ret = poll(&fds[0], fds.size(), -1); // -1 = wait indefinitely
    if (ret < 0) break;

    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].revents & POLLIN) {
            if (fds[i].fd == server_fd) {
                // Accept new connection
            } else {
                // Read from existing client
            }
        }
    }
}
```

### `epoll` (Linux specific, High Performance)
If you choose `epoll`, use `epoll_create`, `epoll_ctl`, `epoll_wait`.
```cpp
#include <sys/epoll.h>

int epoll_fd = epoll_create(1); // Argument is ignored but must be > 0
struct epoll_event event;
event.events = EPOLLIN;
event.data.fd = server_fd;

epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

struct epoll_event events[10];
int n = epoll_wait(epoll_fd, events, 10, -1);
```

### `kqueue`, `kevent` (BSD/MacOS specific)
Similar to epoll but for BSD systems. Use if developing on Mac, but user is on Linux.

### `select`
Older, limited to FD_SETSIZE (usually 1024). Less efficient than poll/epoll.

---

## 3. Data Transmission

### `recv` / `read`
Receive messages from a socket. `recv` allows flags.
```cpp
char buffer[1024];
// MSG_DONTWAIT can be used if socket isn't already non-blocking
ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
if (bytes_read == 0) {
    // Client closed connection
} else if (bytes_read < 0) {
    // Error
}
```

### `send` / `write`
Send messages to a socket.
```cpp
const char *response = "HTTP/1.1 200 OK\r\n\r\nHello";
// ideally check if socket is ready for writing via poll first!
ssize_t bytes_sent = send(client_fd, response, strlen(response), 0);
```

---

## 4. CGI & Process Management

To run scripts (PHP, Python), you must use `fork` and `execve`.

### `fork`
Creates a child process.
```cpp
pid_t pid = fork();
if (pid == 0) {
    // Child process: Execute CGI
} else {
    // Parent process: Handle server loop
}
```

### `execve`
Executes a program. Replaces the current process image.
```cpp
char *args[] = { (char*)"/usr/bin/php-cgi", (char*)"script.php", NULL };
char *env[] = { NULL }; // Construct CGI environment variables here
execve(args[0], args, env);
```

### `pipe`
Creates a unidirectional data channel. Used to capture CGI output.
```cpp
int pipefd[2];
pipe(pipefd);
// pipefd[0] is read end, pipefd[1] is write end
```

### `dup`, `dup2`
Duplicate a file descriptor. Used to redirect stdin/stdout for CGI.
```cpp
// Inside child process:
dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
close(pipefd[0]);
close(pipefd[1]);
```

### `waitpid`
Wait for state change of child process. Use `WNOHANG` for non-blocking server!
```cpp
int status;
// Check if child finished without blocking
pid_t result = waitpid(pid, &status, WNOHANG);
if (result == 0) {
    // Child still running
} else if (result > 0) {
    // Child finished
}
```

### `chdir`
Change working directory. useful for executing CGI in its own folder.
```cpp
chdir("/var/www/html");
```

---

## 5. File System Operations

Used for serving static files and directory listings.

### `stat`
Get file status (size, permissions, type).
```cpp
#include <sys/stat.h>
struct stat file_info;
if (stat("index.html", &file_info) == 0) {
    if (S_ISDIR(file_info.st_mode)) {
        // It's a directory
    }
    off_t file_size = file_info.st_size; // Content-Length
}
```

### `access`
Check real user's permissions for a file.
```cpp
if (access("index.html", R_OK) == 0) {
    // File exists and is readable
}
```

### `opendir`, `readdir`, `closedir`
Read directory contents (for Autoindex feature).
```cpp
#include <dirent.h>
DIR *dir = opendir("./uploads");
struct dirent *entry;
while ((entry = readdir(dir)) != NULL) {
    // entry->d_name is the filename
}
closedir(dir);
```

---

## 6. Network Utilities

### `getaddrinfo`, `freeaddrinfo`
Network address and service translation. Modern replacement for `gethostbyname`.
```cpp
struct addrinfo hints, *res;
memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM;

getaddrinfo("www.example.com", "80", &hints, &res);
// ... use res ...
freeaddrinfo(res);
```

### `inet_addr` (via `arpa/inet.h`)
Converts IP string to integer.
```cpp
in_addr_t ip = inet_addr("127.0.0.1");
```

---

## 7. Utils & Error Handling

### `strerror`, `errno`
Get error message string.
```cpp
#include <cerrno>
#include <cstring>
if (some_call() < 0) {
    std::cerr << "Error: " << strerror(errno) << std::endl;
}
```

### `signal`, `kill`
Handle signals (e.g., Ctrl+C to stop server).
```cpp
#include <csignal>
void handle_sigint(int sig) {
    // Clean up and exit
}
signal(SIGINT, handle_sigint);

// kill sends a signal to a process
kill(child_pid, SIGKILL); 
```

### `gai_strerror`
Error strings for `getaddrinfo`.
```cpp
int status = getaddrinfo(...);
if (status != 0) {
    std::cerr << gai_strerror(status) << std::endl;
}
```
