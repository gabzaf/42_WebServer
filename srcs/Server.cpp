#include "Server.hpp"
#include "CgiHandler.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ; // Needed for execve

Server::Server(const std::vector<ServerConfig> &configs) : _current_tick(0) {
  try {
    initSockets(configs);
  } catch (const std::exception &e) {
    std::cerr << "CRITICAL ERROR in Server constructor: " << e.what() << std::endl;
    throw;
  }
}

Server::~Server() {
  for (size_t i = 0; i < _server_sockets.size(); ++i) {
    close(_server_sockets[i]);
  }
}

void Server::initSockets(const std::vector<ServerConfig> &configs) {
  for (size_t i = 0; i < configs.size(); ++i) {
    int port = configs[i].port;

    // If socket for this port doesn't exist, create it
    if (_port_to_socket.find(port) == _port_to_socket.end()) {
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0)
        throw(std::runtime_error("Failed to create socket"));

      int opt = 1;
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw(std::runtime_error("Failed to set SO_REUSEADDR"));

      struct sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons(port);

      if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw(std::runtime_error("Failed to bind socket"));

      if (listen(fd, 100) < 0) // Increased backlog
        throw(std::runtime_error("Failed to listen on socket"));

      // Set non-blocking
      int flags = fcntl(fd, F_GETFL, 0);
      fcntl(fd, F_SETFL, flags | O_NONBLOCK);

      struct pollfd server_poll;
      server_poll.fd = fd;
      server_poll.events = POLLIN;
      _fds.push_back(server_poll);

      std::cout << "Initializing socket for port " << port << std::endl;
      _server_sockets.push_back(fd);
      _port_to_socket[port] = fd;
      std::cout << "Server listening on port " << port << std::endl;
    }

    // Map the socket FD to this config
    int fd = _port_to_socket[port];
    _socket_configs[fd].push_back(configs[i]);
  }
}

void Server::run() {
  while (true) {
    // 1. Prepare events for poll
    for (size_t i = 0; i < _fds.size(); i++) {
      if (_responses.count(_fds[i].fd))
        _fds[i].events |= POLLOUT;
      else
        _fds[i].events &= ~POLLOUT;
      
      // If we want to close, stop reading
      // If we want to close, keep reading to drain (prevent RST) but don't parse
      // So keep POLLIN active
      // if (_close_requests.count(_fds[i].fd))
      //    _fds[i].events &= ~POLLIN;
    }

    int ret = poll(&_fds[0], _fds.size(), 1000);
    if (ret < 0) {
      throw(std::runtime_error("Poll failed"));
    }

    if (ret == 0) {
      _current_tick++;
    }

    // 2. Process events (using a fixed size to avoid processing newly added fds)
    size_t size = _fds.size();
    for (size_t i = 0; i < size; i++) {
        if (_fds[i].revents == 0) continue;
        int fd = _fds[i].fd;

      if (_fds[i].revents & POLLIN) {
        bool is_server = false;
        for (size_t j = 0; j < _server_sockets.size(); ++j) {
          if (fd == _server_sockets[j]) {
            acceptConnection(fd);
            is_server = true;
            break;
          }
        }
        if (!is_server) {
          handleClient(fd);
          // Check if fd was removed
          bool still_present = false;
          for (size_t k = 0; k < _fds.size(); k++) {
            if (_fds[k].fd == fd) {
              still_present = true;
              break;
            }
          }
          if (!still_present) {
            i--;
            size--;
            continue;
          }
        }
      }

      if (_fds[i].revents & POLLOUT) {
        if (_responses.count(fd)) {
          std::string &res = _responses[fd];
          ssize_t sent = send(fd, res.c_str(), res.length(), 0);
          if (sent <= 0) {
            disconnectClient(fd);
            i--;
            size--;
            continue;
          } else {
            res.erase(0, sent);
            if (res.empty()) {
              _responses.erase(fd);
              if (_close_requests.count(fd)) {
                  disconnectClient(fd);
                  _close_requests.erase(fd);
              }
              // disconnectClient(fd); // Don't always close, only if needed or timeout logic handles it
              continue;
            }
          }
        }
      }
    }
    // Check for timeouts
    for (std::map<int, size_t>::iterator it = _last_activity.begin();
         it != _last_activity.end();) {
      // 60 seconds timeout
      if (_current_tick > it->second && (_current_tick - it->second) > 60) {
        int fd = it->first;
        ++it; // Increment iterator before deletion
        disconnectClient(fd);
      } else
        ++it;
    }
  }
}

void Server::acceptConnection(int server_fd) {
  int client_fd = accept(server_fd, NULL, NULL);

  if (client_fd < 0) {
    if (errno != EWOULDBLOCK)
      std::cerr << "Accept failed" << std::endl;
    return;
  }
  // Set client non-blocking
  int flags = fcntl(client_fd, F_GETFL, 0);
  fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
  struct pollfd client_poll;
  client_poll.fd = client_fd;
  client_poll.events = POLLIN;
  _fds.push_back(client_poll);
  _last_activity[client_fd] = _current_tick;
}

void Server::disconnectClient(int fd) {
  _last_activity.erase(fd);
  _responses.erase(fd);
  _requests.erase(fd);
  _close_requests.erase(fd);
  for (std::vector<struct pollfd>::iterator it = _fds.begin(); it != _fds.end();
       ++it) {
    if (it->fd == fd) {
      _fds.erase(it);
      break;
    }
  }
  close(fd);
}

const ServerConfig &Server::matchServer(int client_fd,
                                        const std::string &host) {
  // 1. Get local port of the client connection
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  if (getsockname(client_fd, (struct sockaddr *)&addr, &len) == -1) {
    return _socket_configs.begin()->second[0];
  }
  int port = ntohs(addr.sin_port);

  // Check if we manage this port
  if (_port_to_socket.find(port) == _port_to_socket.end()) {
    // Fallback safety
    return _socket_configs.begin()->second[0];
  }
  int server_fd = _port_to_socket[port];

  // 2. Look through configs for this socket
  const std::vector<ServerConfig> &configs = _socket_configs[server_fd];

  // Exact match on server_name
  for (size_t i = 0; i < configs.size(); ++i) {
    if (configs[i].server_name == host)
      return configs[i];
  }
  // Fallback to first config for this port (default server)
  return configs[0];
}

void Server::handleClient(int fd) {
  char buffer[8192];
  ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
  
  // If we are scheduled to close, just drain the socket
  if (_close_requests.count(fd)) {
      if (bytes > 0) return; // Discard data
      if (bytes <= 0) {
          disconnectClient(fd);
          _close_requests.erase(fd); // disconnectClient erases it too
          return;
      }
  }

  if (bytes <= 0) {
    disconnectClient(fd);
    return;
  }
  
  _requests[fd].append(buffer, bytes);
  _last_activity[fd] = _current_tick;

  std::string &raw = _requests[fd];
  size_t header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    if (raw.size() > 1000000) { // Safety limit for headers
       sendError(fd, 431, matchServer(fd, "")); 
       disconnectClient(fd);
    }
    return; // Wait for more data
  }

  // Basic check for request completeness (headers + body)
  size_t body_start = header_end + 4;
  
  // Find Content-Length if any
  // Find Content-Length if any
  size_t cl_pos = raw.find("Content-Length:");
  if (cl_pos != std::string::npos && cl_pos < header_end) {
    size_t line_end = raw.find("\r\n", cl_pos);
    if (line_end != std::string::npos) {
      std::string cl_str = raw.substr(cl_pos + 15, line_end - (cl_pos + 15));
      size_t content_length = static_cast<size_t>(std::atol(cl_str.c_str()));
      
      // Check Host header to get correct config
      std::string host = "";
      size_t host_pos = raw.find("Host:");
      if (host_pos != std::string::npos && host_pos < header_end) {
         size_t h_end = raw.find("\r\n", host_pos);
         if (h_end != std::string::npos) {
             host = raw.substr(host_pos + 5, h_end - (host_pos + 5));
             // Trim spaces
             size_t first = host.find_first_not_of(" \t");
             if (first != std::string::npos) {
                 size_t last = host.find_last_not_of(" \t");
                 host = host.substr(first, last - first + 1);
             }
         }
      }

      const ServerConfig &cfg = matchServer(fd, host);
      if (cfg.client_max_body_size > 0 && content_length > cfg.client_max_body_size) {
          sendError(fd, 413, cfg);
          _close_requests.insert(fd);
          return;
      }

      if (raw.size() < body_start + content_length)
        return; // Body incomplete
    }
  }
  
  // Check for chunked transfer encoding
  if (raw.find("Transfer-Encoding: chunked") != std::string::npos && raw.find("Transfer-Encoding: chunked") < header_end) {
    if (raw.find("0\r\n\r\n", body_start) == std::string::npos)
      return; // Chunked body incomplete
  }

  // If we reach here, we have a complete request
  HttpRequest request(raw);
  raw.clear(); // Clear for next request on same connection if persistent
  _requests.erase(fd);

  if (request.getMethod().empty()) {
    disconnectClient(fd);
    return;
  }

  // Match server
  std::string host = request.getHeader("Host");
  size_t colon = host.find(':');
  if (colon != std::string::npos)
    host = host.substr(0, colon);
  const ServerConfig &config = matchServer(fd, host);

  // Match location
  const LocationConfig *loc = matchLocation(request.getPath(), config);

  // Handle redirection
  if (loc && loc->return_loc.first != 0) {
    handleRedirection(fd, loc->return_loc, config);
    return;
  }

  std::string fullPath = resolvePath(request.getPath(), loc, config);

  // Trailing slash redirect for directories
  struct stat s;
  if (stat(fullPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode)) {
    if (!request.getPath().empty() && request.getPath()[request.getPath().size() - 1] != '/') {
      handleRedirection(fd, std::make_pair(301, request.getPath() + "/"), config);
      return;
    }
  }

  // Check allowed methods
  if (!isMethodAllowed(request.getMethod(), loc)) {
    sendError(fd, 405, config);
    return;
  }

  // Dispatch request
  if (request.getMethod() == "GET")
    handleGet(fd, fullPath, request.getPath(), loc, request, config, false);
  else if (request.getMethod() == "HEAD")
    handleGet(fd, fullPath, request.getPath(), loc, request, config, true);
  else if (request.getMethod() == "POST")
    handlePost(fd, request, loc, config);
  else if (request.getMethod() == "DELETE")
    handleDelete(fd, fullPath, loc, config);
  else
    sendError(fd, 501, config);

}


void Server::handleRedirection(int fd,
                               const std::pair<int, std::string> &redirection,
                               const ServerConfig &config) {
  (void)config;
  HttpResponse response;
  response.setStatus(redirection.first, getStatusMsg(redirection.first));
  response.setHeader("Connection", "close");
  response.setHeader("Location", redirection.second);
  std::string rStr = response.toString();
  _responses[fd] = rStr;
}

void Server::handleGet(int fd, const std::string &path, const std::string &uri,
                       const LocationConfig *loc, const HttpRequest &req,
                       const ServerConfig &config, bool headOnly) {
  // 1. Check for CGI
  if (loc && !loc->cgi_ext.empty()) {
    size_t dot = path.find_last_of(".");
    if (dot != std::string::npos && path.substr(dot) == loc->cgi_ext) {
      handleCgi(fd, req, loc, path, config, headOnly);
      return;
    }
  }
  struct stat s;
  if (stat(path.c_str(), &s) == 0 && S_ISDIR(s.st_mode)) {
    if (loc && loc->autoindex) {
      generateAutoindex(fd, path, uri, config, headOnly);
      return;
    } else {
      sendError(fd, 404, config);
      return;
    }
  }
  serveFile(fd, path, config, headOnly);
}

void Server::generateAutoindex(int fd, const std::string &path,
                               const std::string &uri,
                               const ServerConfig &config, bool headOnly) {
  DIR *dir = opendir(path.c_str());

  if (!dir) {
    // Should pass config
    sendError(fd, 500, config);
    return;
  }
  std::stringstream ss;
  ss << "<html><head><title>Index of " << uri << "</title></head><body>";
  ss << "<h1>Index of " << uri << "</h1><hr><pre>";
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    std::string name = entry->d_name;
    if (name == ".")
      continue;
    ss << "<a href=\"";
    if (uri[uri.size() - 1] != '/')
      ss << uri << "/";
    else
      ss << uri;
    ss << name << "\">" << name << "</a>" << std::endl;
  }
  closedir(dir);
  ss << "</pre><hr></body></html>";
  std::string body = ss.str();
  HttpResponse response;
  response.setStatus(200, "OK");
  response.setHeader("Connection", "close");
  response.setHeader("Content-Type", "text/html");
  
  std::stringstream cl_ss;
  cl_ss << body.length();
  response.setHeader("Content-Length", cl_ss.str());

  if (headOnly) {
    _responses[fd] = response.toString();
  } else {
    response.setBody(body);
    _responses[fd] = response.toString();
  }
}


void Server::handleCgi(int fd, const HttpRequest &req,
                       const LocationConfig *loc, const std::string &path,
                       const ServerConfig &config, bool headOnly) {
  if (!loc || loc->cgi_path.empty()) {
    sendError(fd, 500, config);
    return;
  }
  std::string pathInfo = "/";
  if (loc && !loc->path.empty() && loc->path[0] != '~') {
    if (req.getPath().length() >= loc->path.length())
      pathInfo = req.getPath().substr(loc->path.length());
    if (pathInfo.empty() || pathInfo[0] != '/')
      pathInfo = "/" + pathInfo;
  } else if (loc && loc->path[0] == '~') {
    size_t lastSlash = req.getPath().find_last_of('/');
    if (lastSlash != std::string::npos)
      pathInfo = req.getPath().substr(lastSlash);
    else
      pathInfo = "/" + req.getPath();
  }
  CgiHandler cgi(req, path, loc->cgi_path, pathInfo);
  std::string output = cgi.execute();
  if (output.find("Status:") != std::string::npos) {
    size_t pos = output.find("Status:");
    size_t end = output.find("\r\n", pos);
    if (end != std::string::npos && end > pos + 8) {
      std::string status = output.substr(pos + 8, end - (pos + 8));
      output.replace(pos, end - pos, "HTTP/1.1 " + status);
    }
  } else if (output.find("HTTP/1.1") == std::string::npos) {
    output = "HTTP/1.1 200 OK\r\n" + output;
  }
  // Add Connection: close if not already present
  if (output.find("Connection:") == std::string::npos) {
    size_t headerEnd = output.find("\r\n\r\n");
    if (headerEnd != std::string::npos) {
      output.insert(headerEnd, "Connection: close\r\n");
      _close_requests.insert(fd);
    }
  }
  if (headOnly) {
    size_t split = output.find("\r\n\r\n");
    if (split != std::string::npos) {
      // Keep headers but discard body for HEAD
      output = output.substr(0, split + 4);
    }
  }
  _responses[fd] = output;
}

void Server::handlePost(int fd, const HttpRequest &req,
                        const LocationConfig *loc, const ServerConfig &config) {
  std::string targetPath = resolvePath(req.getPath(), loc, config);

  if (config.client_max_body_size > 0 &&
      req.getBody().size() > config.client_max_body_size) {
    sendError(fd, 413, config);
    return;
  }

  if (loc && !loc->cgi_ext.empty()) {
    size_t dot = targetPath.find_last_of(".");
    if (dot != std::string::npos && targetPath.substr(dot) == loc->cgi_ext) {
      handleCgi(fd, req, loc, targetPath, config);
      return;
    }
  }
  if (loc && !loc->upload_store.empty()) {
    std::string fileName = req.getPath();
    size_t lastSlash = fileName.find_last_of("/");
    if (lastSlash != std::string::npos)
      fileName = fileName.substr(lastSlash + 1);
    std::string store = loc->upload_store;
    if (store[store.size() - 1] != '/')
      store += "/";
    targetPath = store + fileName;
  }
  std::ofstream outfile(targetPath.c_str(), std::ios::binary | std::ios::trunc);
  if (!outfile.is_open()) {
    sendError(fd, 500, config);
    return;
  }
  outfile.write(req.getBody().c_str(), req.getBody().length());
  outfile.close();
  HttpResponse response;
  response.setStatus(201, "Created");
  response.setHeader("Connection", "close");
  response.setBody("<html><body><h1>201 Created</h1><p>File uploaded "
                   "successfully.</p></body></html>");
  response.setHeader("Content-Type", "text/html");
  _responses[fd] = response.toString();
}

void Server::handleDelete(int fd, const std::string &path,
                          const LocationConfig *loc,
                          const ServerConfig &config) {
  (void)loc;
  struct stat s;

  if (stat(path.c_str(), &s) != 0) {
    sendError(fd, 404, config);
    return;
  }
  if (S_ISDIR(s.st_mode)) {
    sendError(fd, 403, config);
    return;
  }

  // Use execve /bin/rm instead of forbidden remove()
  pid_t pid = fork();
  if (pid == 0) {
    // Child
    const char *argv[] = {"/bin/rm", "-f", path.c_str(), NULL};
    execve("/bin/rm", (char *const *)argv, environ);
    // If execve fails
    kill(getpid(), SIGKILL);
  } else if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      HttpResponse response;
      response.setStatus(204, "No Content");
      response.setHeader("Connection", "close");
      _responses[fd] = response.toString();
    } else {
      sendError(fd, 500, config);
    }
  } else {
    sendError(fd, 500, config);
  }
}

const LocationConfig *Server::matchLocation(const std::string &path,
                                            const ServerConfig &config) {
  const LocationConfig *bestPrefixMatch = NULL;
  size_t bestPrefixLength = 0;

  // 1. Find the longest prefix match
  for (size_t i = 0; i < config.locations.size(); ++i) {
    const std::string &locPath = config.locations[i].path;
    if (locPath.empty() || locPath[0] == '~')
      continue;

    if (path.compare(0, locPath.length(), locPath) == 0) {
      if (locPath.length() > bestPrefixLength) {
        bestPrefixLength = locPath.length();
        bestPrefixMatch = &config.locations[i];
      }
    } else if (locPath.size() > 1 && locPath[locPath.size() - 1] == '/' &&
               path + "/" == locPath) {
      if (locPath.length() > bestPrefixLength) {
        bestPrefixLength = locPath.length();
        bestPrefixMatch = &config.locations[i];
      }
    }
  }

  // 2. Check for regex matches in order
  for (size_t i = 0; i < config.locations.size(); ++i) {
    const std::string &locPath = config.locations[i].path;
    if (locPath.size() < 2 || locPath[0] != '~')
      continue;

    std::string pattern = locPath.substr(2); // Skip "~ "
    if (pattern.empty()) continue;
    
    // Simplistic regex: check if path ends with pattern (e.g., .bla$)
    if (pattern[pattern.size() - 1] == '$') {
      std::string suffix = pattern.substr(0, pattern.size() - 1);
      if (path.length() >= suffix.length() &&
          path.compare(path.length() - suffix.length(), suffix.length(),
                       suffix) == 0) {
        return (&config.locations[i]);
      }
    } else {
      // General substring match for other regexes
      if (path.find(pattern) != std::string::npos) {
        return (&config.locations[i]);
      }
    }
  }

  return (bestPrefixMatch);
}

bool Server::isMethodAllowed(const std::string &method,
                             const LocationConfig *loc) {
  if (!loc || loc->allow_methods.empty())
    return (true);

  for (size_t i = 0; i < loc->allow_methods.size(); ++i) {
    if (method == loc->allow_methods[i])
      return (true);
  }
  return (false);
}

std::string Server::resolvePath(const std::string &path,
                                const LocationConfig *loc,
                                const ServerConfig &config) {
  std::string root = config.root;
  std::string fullPath;

  if (loc && !loc->root.empty()) {
    root = loc->root;
    if (!root.empty() && root[root.size() - 1] == '/')
      root = root.substr(0, root.size() - 1);
    
    if (!loc->path.empty() && loc->path[0] != '~') {
      size_t matchLen = loc->path.length();
      std::string remainingPath;
      if (path.length() >= matchLen)
          remainingPath = path.substr(matchLen);
      else if (path + "/" == loc->path)
          remainingPath = ""; // Exact match minus trailing slash
      
      if (!remainingPath.empty() && remainingPath[0] != '/' && !root.empty() && root[root.size()-1] != '/')
          fullPath = root + "/" + remainingPath;
      else
          fullPath = root + remainingPath;
    } else {
      fullPath = root + path;
    }
  } else {
    if (root.empty()) root = ".";
    if (!root.empty() && root[root.size() - 1] == '/')
      root = root.substr(0, root.size() - 1);
    fullPath = root + path;
  }

  struct stat s;
  if (stat(fullPath.c_str(), &s) == 0 && S_ISDIR(s.st_mode)) {
    if (loc && !loc->index.empty()) {
      for (size_t i = 0; i < loc->index.size(); ++i) {
        std::string indexPath = fullPath;
        if (indexPath[indexPath.size() - 1] != '/')
          indexPath += "/";
        indexPath += loc->index[i];
        struct stat idxS;
        if (stat(indexPath.c_str(), &idxS) == 0 && S_ISREG(idxS.st_mode))
          return (indexPath);
      }
    }
  }
  return (fullPath);
}

void Server::serveFile(int fd, const std::string &path,
                       const ServerConfig &config, bool headOnly) {
  std::ifstream file(path.c_str(), std::ios::binary);

  if (!file.is_open()) {
    sendError(fd, 404, config);
    return;
  }
  
  // Get file size
  file.seekg(0, std::ios::end);
  std::streampos size = file.tellg();
  file.seekg(0, std::ios::beg);
  
  HttpResponse response;
  response.setStatus(200, "OK");
  response.setHeader("Content-Type", getContentType(path));
  response.setHeader("Connection", "close");
  
  if (headOnly) {
    // For HEAD: send headers with Content-Length but NO body
    std::stringstream ss;
    ss << size;
    response.setHeader("Content-Length", ss.str());
    _responses[fd] = response.toString();
    return;
  }
  
  // For GET: read file and send with body
  std::stringstream content;
  content << file.rdbuf();
  response.setBody(content.str());
  std::string rStr = response.toString();
  _responses[fd] = rStr;
}

std::string Server::getContentType(const std::string &path) {
  size_t dot = path.find_last_of(".");

  if (dot == std::string::npos)
    return ("text/plain");
  std::string ext = path.substr(dot + 1);
  if (ext == "html" || ext == "htm")
    return ("text/html");
  if (ext == "css")
    return ("text/css");
  if (ext == "js")
    return ("application/javascript");
  if (ext == "png")
    return ("image/png");
  if (ext == "jpg" || ext == "jpeg")
    return ("image/jpeg");
  return ("text/plain");
}

void Server::sendError(int fd, int code, const ServerConfig &config) {
  HttpResponse response;
  std::string msg = getStatusMsg(code);

  response.setStatus(code, msg);
  response.setHeader("Connection", "close");
  // Check for custom error page in config
  std::map<int, std::string>::const_iterator it = config.error_pages.find(code);
  if (it != config.error_pages.end()) {
    std::string root = config.root.empty() ? "." : config.root;
    std::string path = root + "/" + it->second;
    std::ifstream file(path.c_str());
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      response.setBody(buffer.str());
      response.setHeader("Content-Type", "text/html");
      _responses[fd] = response.toString();
      return;
    }
  }
  // Fallback internal error page
  std::stringstream ss;
  ss << "<html><head><title>" << code << " " << msg << "</title></head>";
  ss << "<body><center><h1>" << code << " " << msg << "</h1></center>";
  ss << "<hr><center>Webserv/1.0</center></body></html>";
  response.setBody(ss.str());
  response.setHeader("Content-Type", "text/html");
  _responses[fd] = response.toString();
}

std::string Server::getStatusMsg(int code) {
  switch (code) {
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 204:
    return "No Content";
  case 301:
    return "Moved Permanently";
  case 400:
    return "Bad Request";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 413:
    return "Content Too Large";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  case 505:
    return "HTTP Version Not Supported";
  default:
    return "Unknown Error";
  }
}
