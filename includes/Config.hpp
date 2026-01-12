#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

class	LocationConfig
{
	public:
		std::string			path;
		std::string			root;
		std::vector<std::string>	index;
		bool				autoindex;
		std::vector<std::string>	allow_methods;
		std::string			cgi_ext;
		std::string			cgi_path;
		std::pair<int, std::string>	return_loc;
		std::string			upload_store;

		LocationConfig();
		~LocationConfig();
};

class	ServerConfig
{
	public:
		int				port;
		std::string			host;
		std::string			server_name;
		std::string			root;
		unsigned long			client_max_body_size;
		std::map<int, std::string>	error_pages;
		std::vector<LocationConfig>	locations;

		ServerConfig();
		~ServerConfig();
};

class	Config
{
	private:
		void	parse(const std::string &filename);
		void	parseServer(std::ifstream &file, std::string &line);
		void	parseLocation(std::ifstream &file, std::string &line, ServerConfig &server);

		void	handleSrvListen(std::stringstream &ss, ServerConfig &server);
		void	handleSrvHost(std::stringstream &ss, ServerConfig &server);
		void	handleSrvServerName(std::stringstream &ss, ServerConfig &server);
		void	handleSrvRoot(std::stringstream &ss, ServerConfig &server);
		void	handleSrvClientBodySize(std::stringstream &ss, ServerConfig &server);
		void	handleSrvErrorPage(std::stringstream &ss, ServerConfig &server);
		void	handleSrvCgi(std::stringstream &ss, ServerConfig &server);

		void	handleLocRoot(std::stringstream &ss, LocationConfig &loc);
		void	handleLocAutoindex(std::stringstream &ss, LocationConfig &loc);
		void	handleLocIndex(std::stringstream &ss, LocationConfig &loc);
		void	handleLocAllowMethods(std::stringstream &ss, LocationConfig &loc);
		void	handleLocReturn(std::stringstream &ss, LocationConfig &loc);
		void	handleLocCgiExt(std::stringstream &ss, LocationConfig &loc);
		void	handleLocCgiPath(std::stringstream &ss, LocationConfig &loc);
		void	handleLocUploadStore(std::stringstream &ss, LocationConfig &loc);

	public:
		std::vector<ServerConfig>	_servers;

		Config(const std::string &filename);
		~Config();

		const std::vector<ServerConfig> &getServers() const;

		typedef void (Config::*ServerDirectiveHandler)(std::stringstream &, ServerConfig &);
		typedef void (Config::*LocationDirectiveHandler)(std::stringstream &, LocationConfig &);
};

#endif
