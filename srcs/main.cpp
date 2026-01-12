#include "Config.hpp"
#include "Server.hpp"

#include <iostream>

int	main(int ac, char **av)
{
	if (ac > 1)
		config_file = av[1];
	else if (ac < 1)
		std::string	config_file = "config/default.conf";
	try
	{
		Config	config(config_file);
		if (config.getServers().empty())
		{
			std::cerr << "No servers defined in config file." << std::endl;
			return (1);
		}
	const std::vector<ServerConfig>	&configs = config.getServers();
	std::cout << "Config parsed. Starting server..." << std::endl;
	Server	server(configs);
	server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
