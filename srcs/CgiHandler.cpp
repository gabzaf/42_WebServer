#include "CgiHandler.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <fcntl.h>

CgiHandler::CgiHandler(const HttpRequest &request,
                       const std::string &scriptPath,
                       const std::string &interpreterPath,
                       const std::string &pathInfo)
    : _request(request), _scriptPath(scriptPath),
      _interpreterPath(interpreterPath), _pathInfo(pathInfo) {
  setupEnv();
}

CgiHandler::~CgiHandler() {}

void CgiHandler::setupEnv() {
  char abs_path[4096];
  char *res_path = realpath(_scriptPath.c_str(), abs_path);
  std::string full_script_path = res_path ? res_path : _scriptPath;

  _env["REQUEST_METHOD"] = _request.getMethod();
  _env["SCRIPT_FILENAME"] = full_script_path;
  _env["SCRIPT_NAME"] = _request.getPath();
  _env["PATH_INFO"] = _request.getPath();
  _env["PATH_TRANSLATED"] = full_script_path;
  _env["QUERY_STRING"] = _request.getQueryString();
  std::string cl = _request.getHeader("Content-Length");
  _env["CONTENT_LENGTH"] = cl.empty() ? "0" : cl;
  _env["CONTENT_TYPE"] = _request.getHeader("Content-Type");
  _env["GATEWAY_INTERFACE"] = "CGI/1.1";
  _env["SERVER_PROTOCOL"] = "HTTP/1.1";
  _env["REDIRECT_STATUS"] = "200";
  _env["SERVER_SOFTWARE"] = "Webserv/1.0";
  std::string host = _request.getHeader("Host");
  if (!host.empty()) {
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
      _env["SERVER_NAME"] = host.substr(0, colon);
      _env["SERVER_PORT"] = host.substr(colon + 1);
    } else {
      _env["SERVER_NAME"] = host;
      _env["SERVER_PORT"] = "1250"; // Fallback to tester port
    }
  } else {
    _env["SERVER_NAME"] = "localhost";
    _env["SERVER_PORT"] = "1250";
  }
  _env["REMOTE_ADDR"] = "127.0.0.1";
  
  // Add all request headers with HTTP_ prefix
  const std::map<std::string, std::string>& headers = _request.getHeaders();
  for (std::map<std::string, std::string>::const_iterator it = headers.begin();
       it != headers.end(); ++it) {
    std::string key = it->first;
    // Convert to uppercase and replace - with _
    for (size_t i = 0; i < key.length(); ++i) {
      if (key[i] == '-') key[i] = '_';
      else key[i] = std::toupper(key[i]);
    }
    _env["HTTP_" + key] = it->second;
  }
}

char **CgiHandler::getEnvAsCstrArray() const {
  char **envp = new char *[_env.size() + 1];
  int i = 0;
  for (std::map<std::string, std::string>::const_iterator it = _env.begin();
       it != _env.end(); ++it) {
    std::string envStr = it->first + "=" + it->second;
    envp[i] = new char[envStr.size() + 1];
    for (size_t j = 0; j < envStr.size(); j++)
      envp[i][j] = envStr[j];
    envp[i][envStr.size()] = '\0';
    i++;
  }
  envp[i] = NULL;
  return (envp);
}

void CgiHandler::freeEnvCstrArray(char **envp) const {
  for (int i = 0; envp[i] != NULL; i++)
    delete[] envp[i];
  delete[] envp;
}

std::string CgiHandler::execute() {
  int pipe_in[2];
  int pipe_out[2];

  if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
    return ("Status: 500 Internal Server Error\r\n\r\nCGI pipe failed");
  pid_t pid = fork();
  if (pid < 0)
    return ("Status: 500 Internal Server Error\r\n\r\nCGI fork failed");
  if (pid == 0) // child
  {
    close(pipe_in[1]);
    close(pipe_out[0]);
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    close(pipe_in[0]);
    close(pipe_out[1]);
    char abs_path[4096];
    char *res_path = realpath(_interpreterPath.c_str(), abs_path);

    char abs_script[4096];
    char *res_script = realpath(_scriptPath.c_str(), abs_script);
    std::string full_script_path = res_script ? res_script : _scriptPath;

    // Change directory to the script's directory for relative file access
    std::string script_dir = full_script_path.substr(0, full_script_path.find_last_of('/'));
    chdir(script_dir.c_str());

    char **envp = getEnvAsCstrArray();
    char *argv[3];
    argv[0] = res_path ? res_path : (char *)_interpreterPath.c_str();
    argv[1] = (char *)full_script_path.c_str();
    argv[2] = NULL;
    execve(argv[0], argv, envp);
    // If execve fails
    freeEnvCstrArray(envp);
    exit(1);
  } else // Parent
  {
    close(pipe_in[0]);
    close(pipe_out[1]);
    // Write body if POST
    if (_request.getMethod() == "POST") {
      std::string body = _request.getBody();
      write(pipe_in[1], body.c_str(), body.length());
    }
    close(pipe_in[1]);
    // Non-blocking read and timeout handling
    std::string output;
    char buffer[1024];
    ssize_t bytes;
    int timeout_ms = 10000; // 10 seconds
    int elapsed_ms = 0;
    int status = 0;
    bool exited = false;

    while (elapsed_ms < timeout_ms) {
      pid_t res = waitpid(pid, &status, WNOHANG);
      if (res == pid) {
        exited = true;
        break;
      } else if (res < 0) {
        // Error or already reaped
        exited = true; // Assume exited if we can't wait
        status = 256; // Treat as error (exit 1)
        break; 
      }

      struct pollfd pfd;
      pfd.fd = pipe_out[0];
      pfd.events = POLLIN;
      int poll_res = poll(&pfd, 1, 100);
      if (poll_res > 0) { 
        bytes = read(pipe_out[0], buffer, sizeof(buffer) - 1);
        if (bytes > 0) {
          buffer[bytes] = '\0';
          output += buffer;
        } else if (bytes == 0) {
           // Pipe closed, output finished. Wait for exit.
           waitpid(pid, &status, 0);
           exited = true;
           break;
        }
      } else if (poll_res < 0) {
         // Poll error
      }
      elapsed_ms += 100;
    }

    // Drain remaining output
    int flags = fcntl(pipe_out[0], F_GETFL, 0);
    fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);
    while ((bytes = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes] = '\0';
      output += buffer;
    }

    if (!exited) {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return ("Status: 504 Gateway Timeout\r\n\r\nCGI script timed out");
    }

    // Capture remaining output after exit
    while ((bytes = read(pipe_out[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes] = '\0';
      output += buffer;
    }
    close(pipe_out[0]);

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
      return ("Status: 500 Internal Server Error\r\n\r\nCGI script exited with "
              "error");
    
    // Check if output contains headers
    if (output.find("Content-Type:") == std::string::npos && output.find("Status:") == std::string::npos && output.find("\r\n\r\n") == std::string::npos)
        return ("Status: 500 Internal Server Error\r\n\r\nCGI did not produce valid headers");

    return (output);
  }
}
