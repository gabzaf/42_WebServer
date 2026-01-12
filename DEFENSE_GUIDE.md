# Webserv Defense Guide

This document maps the evaluation checklist to the codebase to assist during the defense.

## Mandatory Part

### I/O Multiplexing & Main Loop
*   **Requirement**: Use `poll` (or equivalent), single loop, check Read/Write simultaneously.
*   **Code Location**: `srcs/Server.cpp` -> `Server::run()` (Lines ~80-160).
*   **Evidence**:
    *   We use `std::vector<struct pollfd> _fds`.
    *   We set `POLLIN` and `POLLOUT` flags based on state (`_responses` map logic).
    *   We call `poll()` once per iteration.
    *   We iterate through `_fds` and check `revents & POLLIN` and `revents & POLLOUT`.

### Client Handling
*   **Requirement**: Only one read or one write per client per select/poll.
*   **Code Location**: `srcs/Server.cpp`.
*   **Evidence**:
    *   Reading: `handleClient` calls `recv` **once** when `POLLIN` is active.
    *   Writing: `Server::run` calls `send` **once** when `POLLOUT` is active.
*   **Requirement**: Client removed on error.
*   **Evidence**: `disconnectClient(fd)` is called on `recv <= 0` or `send <= 0`.

### Configuration
*   **ConfigFile**: `config/evalConfigFile.conf`
*   **Verification Commands**:
    *   **Multiple Ports**: `./webserv config/evalConfigFile.conf` opens ports 1234, 1235, etc.
        *   Test: `curl http://localhost:1234` vs `curl http://localhost:1235`
    *   **Multiple Hostnames**: Ports 1236 shares `sameport1` and `sameport2`.
        *   Test: `curl --resolve sameport1:1236:127.0.0.1 http://sameport1:1236`
    *   **Error Page**: Port 1237 has custom 404.
        *   Test: `curl -v http://localhost:1237/missing` -> Returns custom HTML.
    *   **Body Limit**: Port 1237 limit 10 bytes. Port 1250 limit 100 bytes.
        *   Test: `curl -X POST -d "12345678901" http://localhost:1237/` -> Returns 413.
    *   **Routes/Index**: Port 1238 `/someLocation/`.
    *   **Methods**: Port 1239.
        *   Test: `curl -X DELETE http://localhost:1239/` -> Returns 405 (Method Not Allowed).

### CGI
*   **Requirement**: Relative path access, GET/POST, Error handling.
*   **Code Location**: `srcs/CgiHandler.cpp`.
*   **Evidence**:
    *   **Relative Paths**: Fixed by `chdir(script_dir)` in child process (Line ~115).
    *   **Infinite Loop**: Handled by timeout loop in `execute` (returns 504).
    *   **Execution Errors**: Handled by exit code check (returns 500).
*   **Tests**:
    *   `./test_eval_config.sh` runs specific CGI tests.

## Evaluation Checklist - Interactive Tests

### Basic Checks
Run these during defense:
1.  **GET**: `curl -v http://127.0.0.1:1250/`
2.  **POST**: `curl -v -X POST -d "data" http://127.0.0.1:1250/post_body`
3.  **DELETE**: `curl -v -X DELETE http://127.0.0.1:1250/uploaded_file` (if file exists)
4.  **Unknown Method**: `curl -v -X BLABLA http://127.0.0.1:1250/` -> 405 or 501.

### Browser Test
1.  Open Firefox/Chrome.
2.  Go to `http://localhost:1250/`.
3.  Check Network Tab for Headers.
4.  Try `http://localhost:1250/directory/` for autoindex (if enabled) or index file.
5.  Try `http://localhost:1241/google` for 301 Redirection.

### Siege & Stress Test
*   **Availability**:
    ```bash
    siege -b -t 10S http://127.0.0.1:1250/
    ```
    *   Target: 99.5% availability.
*   **Memory Leaks**:
    *   While siege is running, check `top` or `htop`. Memory usage should remain stable.
    *   We use C++ containers (`std::vector`, `std::map`) which manage memory automatically.
    *   `CgiHandler` ensures `envp` array is deleted.

## Known Design Choices
1.  **Synchronous CGI**: Ideally, CGI should be fully non-blocking. In this implementation, the main loop delegates to `CgiHandler::execute`, which waits for the script (with a timeout). This is a common design trade-off for simplicity in this project scope, though it pauses 'other' clients during the CGI execution time.
2.  **Request Buffering**: We buffer the full request header before processing to ensure validity, but check `Content-Length` immediately to reject oversized bodies early (fixing the 413 "short write" issue).
