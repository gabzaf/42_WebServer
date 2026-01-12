# Webserv Project Roadmap

- [x] **Phase 1: Foundation & Core Components**
    - [x] **Config Parser**: Define syntax, implement parsing logic, validate config.
    - [x] **Socket & Networking**: Wrapper classes for socket creation, binding, listening.
    - [x] **HTTP Protocol**: Request parser (headers, body, methods) and Response builder.
    - [x] **Event Loop**: Main server loop using `poll` or `epoll` handling non-blocking I/O.

- [x] **Phase 2: Core Integration**
    - [x] Connect Event Loop with Request Parser.
    - [x] Implement simple static file serving (GET).
    - [x] Handle client disconnection and timeout.

- [x] **Phase 3: Advanced Features**
    - [x] **POST & DELETE**: File uploads and resource deletion.
    - [x] **CGI Execution**: Architecture for running PHP/Python scripts.
    - [x] **Response Handling**: Error pages (404, 500, etc.) and Autoindex.

- [x] **Phase 4: Stress Testing & Refinement**
    - [x] Siege testing / multiple clients.
    - [x] Memory leak checks (Manual Audit).
    - [x] Compliance checks (RFC standards).
