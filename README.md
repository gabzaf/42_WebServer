# webserv

HTTP/1.1 server in C++98, written for 42's **webserv** subject.

It parses an nginx-style config, listens on one or more ports, and serves requests from a single `poll()` event loop: static files, uploads, DELETE, CGI, autoindex, redirects, custom error pages, and virtual hosts.

---

## Quick start

```bash
make
./webserv config/evalConfigFile.conf
```

Then in another terminal:

```bash
curl -v http://127.0.0.1:1250/
./test_eval_config.sh          # needs the server already running
```

`make` builds `./webserv` (C++98, `-Wall -Wextra -Werror`). There is no default config file named `default.conf` — always pass a path. The evaluation config is the one you want.

---

## Folder map

```
WSToPresent/
├── srcs/                    C++ sources (the server)
│   ├── main.cpp             parse config → Server::run()
│   ├── Server.cpp           poll loop, routing, GET/POST/DELETE, files
│   ├── Config.cpp           .conf parser (dispatch tables)
│   ├── HttpRequest.cpp      request parser (method, URI, headers, body)
│   ├── HttpResponse.cpp     status / headers / body → wire string
│   └── CgiHandler.cpp       fork + pipes + poll + execve
├── includes/                matching headers
├── config/
│   ├── evalConfigFile.conf  **use this** — eval checklist (ports 1234–1250)
│   ├── configFileDefault.conf
│   ├── configFileV3.conf
│   ├── stress_test.conf
│   ├── claudeConfig.conf
│   └── duplicate_port.conf
├── www/                     default static site
├── tests/                   HTML fixtures + cgi scripts for evalConfig
├── cgi-bin/                 extra CGI scripts (test / error / infinite)
├── dumpster/                POST/DELETE playground
├── YoupiBanane/             42 tester document root
├── test_www/                extra static roots
├── notes/                   how it was built (see below)
├── DEFENSE_GUIDE.md         eval checklist → exact files/lines
├── evaluation_qa.md         oral-defense Q&A
├── test_eval_config.sh      curl suite against evalConfigFile.conf
├── stress_test*.sh          concurrent curl hammers
├── tester / cgi_tester      42 official testers (mac-ish binaries)
├── ubuntu_tester /
│   ubuntu_cgi_tester        same testers, Linux builds
└── en.subject.pdf           the 42 subject PDF
```

---

## How a request is handled

```
./webserv config.conf
        │
        ▼
   Config parser          ServerConfig + LocationConfig vectors
        │
        ▼
   Server::initSockets    one non-blocking listen fd per unique port
        │                 several ServerConfig can share a port (vhost)
        ▼
   Server::run()          while (true) { poll(); ... }
        │
        ├─ POLLIN  on listen fd  → accept(), add client to _fds
        ├─ POLLIN  on client fd  → recv() once → parse → route
        └─ POLLOUT on client fd  → send() once from _responses buffer
```

Routing lives in `Server.cpp`:

1. `matchServer(fd, Host)` — pick the `server` block (port + `server_name`)
2. `matchLocation(uri)` — longest matching `location`
3. `isMethodAllowed` — else **405**
4. CGI if the location has `cgi_ext` / `cgi_path`
5. else GET / HEAD / POST / DELETE / `return` redirect
6. `sendError` for 4xx/5xx, using `error_page` when configured

Clients that sit idle are dropped after ~60s (`_last_activity` / `_current_tick`; poll timeout is 1s).

---

## What was implemented

Checked off in `notes/task.md`. Rough timeline from the two repos:

**Phase 1 — foundation** (`42_webServ`)

- Config parser (`server` / `location` blocks, dispatch tables)
- Non-blocking sockets, `poll()` loop
- `HttpRequest` / `HttpResponse`

**Phase 2 — static serving**

- GET, index files, MIME types
- Client timeout + `disconnectClient`

**Phase 3 — methods & errors** 

- POST (upload) and DELETE
- Autoindex HTML
- Centralized error pages

**Phase 4 — eval polish**

- Multi-port + virtual hosts
- CGI: env, relative paths, POST body, timeout, poll errors
- Indentation / Norm-ish cleanup
- Testers, defense docs, `test_eval_config.sh`

### Status codes you can expect

| Code | When |
|------|------|
| 200 | GET/HEAD success |
| 201 | POST created a file |
| 204 | DELETE success |
| 301 | `return` redirect |
| 400 | malformed request |
| 403 | forbidden / listing disabled |
| 404 | missing file (custom page if `error_page` set) |
| 405 | method not in `allow_methods` |
| 413 | body > `client_body_size` |
| 500 | CGI crash / exec failure |
| 501 | unknown method |
| 504 | CGI timeout (e.g. `infinite.py`) |

---

## Config cheat sheet

Nginx-like. Directives end with `;`. Blocks use `{ }`.

```nginx
server {
    listen 1250;
    server_name eval;
    client_body_size 100;          # bytes
    error_page 404 custom404.html;
    root www/;

    location / {
        allow_methods GET;
        index index.html;
        autoindex off;
    }

    location /dumpster/ {
        allow_methods POST DELETE;
        upload_store dumpster/;
    }

    location /google {
        return 301 http://www.google.com;
    }

    location ~ \.bla$ {
        cgi_ext .bla;
        cgi_path ./cgi_tester;
        allow_methods GET POST;
    }
}
```

Two `server` blocks can share a `listen` port if `server_name` differs. The first block for that port is the default when `Host` matches nothing.

---

## Eval config ports (`config/evalConfigFile.conf`)

| Port | What it proves | Quick test |
|------|----------------|------------|
| **1234** | server 1, GET | `curl http://localhost:1234/` |
| **1235** | server 2, other port | `curl http://localhost:1235/` |
| **1236** | two hostnames, same port | `curl -H "Host: sameport1" http://localhost:1236/` |
| **1237** | custom 404 + 10-byte body limit | `curl http://localhost:1237/missing` → custom 404; POST 11+ bytes → 413 |
| **1238** | `location /someLocation/` → `tests/` | `curl http://localhost:1238/someLocation/` |
| **1239** | `allow_methods`; POST/DELETE in `/dumpster/` | `curl -X DELETE http://localhost:1239/` → 405 |
| **1240** | CGI (python3) | `curl http://localhost:1240/tests/cgi/test.py?name=x` |
| **1241** | redirect + autoindex | `curl -I http://localhost:1241/google` → 301; browse `/tests/` |
| **1250** | main eval server (GET `/`, POST `/post_body`, `.bla` CGI, `/directory/` → YoupiBanane) | `curl http://127.0.0.1:1250/` |

Full scripted pass: start the server with `evalConfigFile.conf`, then `./test_eval_config.sh`.

### CGI scripts

| Script | Expected |
|--------|----------|
| `tests/cgi/test.py` | 200, prints query / POST body / `data.txt` |
| `tests/cgi/infinite.py` | **504** (timeout) |
| `tests/cgi/error.py` | **500** |

### Stress

```bash
siege -b -t 10S http://127.0.0.1:1250/
```

Target: ~99.5%+ availability, RSS should stay flat. There are also `stress_test.sh`, `stress_test_post.sh`, `stress_test_cgi.sh` (check the URL/port inside — some still say `:8080`).

---

### Design trade-off to say out loud

CGI is **synchronous from the main loop's point of view**: `CgiHandler::execute()` waits on the child (with a `poll` timeout on the pipes). Other clients pause for that CGI. Fork still isolates crashes. This was an intentional simplicity trade-off.

---

## Notes (how past-you learned this)

| File | What's in it |
|------|----------------|
| `notes/task.md` | Phase checklist (all done) |
| `notes/implementation_plan.md` | Original split: "Container" (sockets/`poll`) vs "Content" (HTTP) |
| `notes/tutorial.md` | Early walkthrough of Config + Server (written after phase 1) |
| `notes/function_reference.md` | syscalls / helpers |
| `notes/http1.0.txt` | protocol notes |
| `notes/resources.txt` | RFC 1945, other 42 webserv writeups |

The subject PDF is `en.subject.pdf`.

---

## Build / clean

```bash
make          # → ./webserv
make clean    # objects
make fclean   # objects + binary
make re
```
