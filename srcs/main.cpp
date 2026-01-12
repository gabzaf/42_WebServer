#include "Config.hpp"
#include "Server.hpp"
#include <iostream>

int main(int ac, char **av) {
  std::string config_file = "config/default.conf";
  if (ac > 1)
    config_file = av[1];

  try {
    Config config(config_file);

    // For Phase 1, we just pick the first server found in config
    if (config.getServers().empty()) {
      std::cerr << "No servers defined in config file." << std::endl;
      return 1;
    }

    const std::vector<ServerConfig> &configs = config.getServers();
    std::cout << "Config parsed. Starting server..." << std::endl;
    Server server(configs);
    server.run();

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
