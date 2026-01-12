# Implementation Plan - Webserv

## Team Split Strategy
To maximize parallel work and minimize merge conflicts, we will split the architecture into **"The Container" (Networking/Core)** and **"The Content" (HTTP/Parsing)**.

### Developer A: The "Network Architect"
**Focus:** Managing the server lifecycle, connections, and raw data flow.
- **Responsibilities:**
    - **Config Parsing**: Reading the `.conf` file and storing it in a structured `ServerConfig` object.
    - **Socket Management**: Creating sockets, binding, listening, and setting them to non-blocking mode.
    - **The Event Loop**: Implementing `poll()` (or `epoll`). Monitoring FDs for read/write readiness.
    - **Client Lifecycle**: Accepting connections, tracking active `Client` objects, detecting timeouts, and closing connections.
    - **CGI "Plumbing"**: Implementing the `fork`, `execve`, `pipe`, `dup2` logic to run scripts.

### Developer B: The "Protocol Specialist"
**Focus:** Making sense of the data and generating the correct response.
- **Responsibilities:**
    - **HTTP Request Parser**: Parsing raw bytes from the buffer into a `Request` object (Method, URI, Headers, Body). Handling chunked transfer encoding is a major task here.
    - **HTTP Response Builder**: Creating a `Response` object that formats the output (Status line, Headers, Body).
    - **Routing Logic**: Deciding *what* to do based on the URI and Config (matching `location` blocks).
    - **Method Implementation**: Logic for `GET` (reading files), `POST` (uploading/processing), `DELETE`.
    - **Autoindex & Error Pages**: Generating HTML for directory listings and errors.

## Proposed Module Structure

### 1. Networking Layer (Dev A)
- `class Server`: Represents a listening server (IP/Port specific).
- `class Client`: Represents a connected client. Holds raw buffers and the current `Request`/`Response` objects.
- `class Cluster`: Manages multiple `Server` instances and the main `poll` loop.

### 2. Protocol Layer (Dev B)
- `class HttpRequest`: Parses and stores request data.
- `class HttpResponse`: Constructs the raw response string.
- `class Location`: Represents a specific route configuration.

---

## Phase 1: The "Hello World" Foundation
- **Dev A**: Write a simple TCP server using `poll` that can accept connections and print "Client Connected" to stdout. Implement `ServerConfig` class.
    - [x] Config Parser implemented with V3 features (server, location, limits, errors, CGI).
- **Dev B**: Write a standalone parser that takes a raw HTTP string and prints the parsed Method, Path, and Headers.

## Phase 2: Integration
- Connect the two parts.
- **Flow**: Dev A's Event Loop reads bytes -> passes to Dev B's Parser -> Parser returns "Request Complete" -> Dev B's Response Builder creates a "200 OK" message -> Dev A's Event Loop sends it back.

## Phase 3: The Heavy Lifting
- **Dev A**: Implement CGI execution logic (piping data in/out of child processes).
- **Dev B**: Implement `POST` (handling large bodies/uploads) and `DELETE`.

## Phase 4: Refinement
- Stress testing, memory leak fixes, and compliance with specific NGINX behaviors.
