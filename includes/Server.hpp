#pragma once

#include "Config.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <vector>
#include <set>

class HttpRequest;

class Server {
private:
  // Map of port -> listening_socket_fd
  std::map<int, int> _port_to_socket;
  // Map of listening_socket_fd -> vector of ServerConfig
  std::map<int, std::vector<ServerConfig> > _socket_configs;

  std::vector<int> _server_sockets; // List of all listening fds

  //struct sockaddr_in _address;
  std::vector<struct pollfd> _fds;
  std::map<int, std::string> _responses; // Response buffer per client
  std::map<int, std::string> _requests;  // Request buffer per client
  std::set<int> _close_requests;       // Clients to close after sending response

  // Timeout management
  std::map<int, size_t> _last_activity;
  size_t _current_tick;

  void initSockets(const std::vector<ServerConfig> &configs);
  void acceptConnection(int server_fd);
  void disconnectClient(int fd);
  void handleClient(int fd);

  const ServerConfig &matchServer(int fd, const std::string &host);
  const LocationConfig *matchLocation(const std::string &path,
                                      const ServerConfig &config);

  void handleGet(int fd, const std::string &path, const std::string &uri,
                 const LocationConfig *loc, const HttpRequest &req,
                 const ServerConfig &config, bool headOnly = false);
  void handlePost(int fd, const HttpRequest &req, const LocationConfig *loc,
                  const ServerConfig &config);
  void handleDelete(int fd, const std::string &path, const LocationConfig *loc,
                    const ServerConfig &config);
  void handleCgi(int fd, const HttpRequest &req, const LocationConfig *loc,
                 const std::string &path, const ServerConfig &config,
                 bool headOnly = false);
  void handleRedirection(int fd, const std::pair<int, std::string> &redirection,
                         const ServerConfig &config);

  void serveFile(int fd, const std::string &path, const ServerConfig &config,
                 bool headOnly = false);
  void sendError(int fd, int code, const ServerConfig &config);
  std::string getStatusMsg(int code);
  std::string getContentType(const std::string &path);
  void generateAutoindex(int fd, const std::string &path,
                         const std::string &uri, const ServerConfig &config,
                         bool headOnly = false);
  std::string resolvePath(const std::string &path, const LocationConfig *loc,
                          const ServerConfig &config);
  bool isMethodAllowed(const std::string &method, const LocationConfig *loc);

public:
  Server(const std::vector<ServerConfig> &configs);
  ~Server();
  void run();
};
