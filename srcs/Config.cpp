#include "Config.hpp"

LocationConfig::LocationConfig() : autoindex(false), return_loc(std::make_pair(0, "")){}

LocationConfig::~LocationConfig(){}

ServerConfig::ServerConfig() : port(80), host("0.0.0.0"), root("."), client_max_body_size(1000000){}

ServerConfig::~ServerConfig(){}

Config::Config(const std::string &filename){parse(filename);}

Config::~Config(){}

static std::string		trim(const std::string &str)
{
	size_t	first = str.find_first_not_of(" \t\r\n");
	if (std::string::npos == first)
		return ("");
	size_t last = str.find_last_not_of(" \t\r\n");
	return (str.substr(first, (last - first + 1)));
}

static std::string		removeSemicolon(std::string str)
{
	if (!str.empty() && str[str.size() - 1] == ';')
		return (str.substr(0, str.size() - 1));
	return (str);
}

const std::vector<ServerConfig>	&Config::getServers() const
{
 	return (_servers);
}

void				Config::parse(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw(std::runtime_error("Could not open config file: " + filename));
	std::string line;
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line == "server" || line == "server{")
			parseServer(file, line);
		else if (line.find("server {") != std::string::npos)
			parseServer(file, line);
	}
}

void				Config::handleSrvListen(std::stringstream &ss, ServerConfig &server)
{
	ss >> server.port;
}

void				Config::handleSrvHost(std::stringstream &ss, ServerConfig &server)
{
	ss >> server.host;
	server.host = removeSemicolon(server.host);
}

void				Config::handleSrvServerName(std::stringstream &ss, ServerConfig &server)
{
	ss >> server.server_name;
	server.server_name = removeSemicolon(server.server_name);
}

void				Config::handleSrvRoot(std::stringstream &ss, ServerConfig &server)
{
	ss >> server.root;
	server.root = removeSemicolon(server.root);
}

void				Config::handleSrvClientBodySize(std::stringstream &ss, ServerConfig &server)
{
	ss >> server.client_max_body_size;
}

void				Config::handleSrvErrorPage(std::stringstream &ss, ServerConfig &server)
{
	int		code;
	std::string	page;
	ss >> code >> page;
	server.error_pages[code] = removeSemicolon(page);
}

void				Config::handleSrvCgi(std::stringstream &ss, ServerConfig &server)
{
	(void)ss;
	(void)server;
}

void				Config::parseServer(std::ifstream &file, std::string &line)
{
	ServerConfig				server;
	static const std::string		keys[] = {"listen", "host", "server_name", "root", "client_body_size", "error_page", "cgi"};
	static const ServerDirectiveHandler 	handlers[] = {&Config::handleSrvListen, &Config::handleSrvHost, &Config::handleSrvServerName, &Config::handleSrvRoot,
								&Config::handleSrvClientBodySize, &Config::handleSrvErrorPage, &Config::handleSrvCgi};
	const int				count = sizeof(keys) / sizeof(keys[0]);
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line == "}")
			break;
		if (line == "{")
			continue;
		std::stringstream	ss(line);
		std::string		key;
		ss >> key;
		if (key == "location")
		{
			parseLocation(file, line, server);
			continue;
		}
		bool	handled = false;
		for (int i = 0; i < count; ++i)
		{
			if (key == keys[i])
			{
				(this->*handlers[i])(ss, server);
				handled = true;
				break;
			}
		}
		if (!handled)
			throw(std::runtime_error("Unknown server directive: " + key));
	}
	_servers.push_back(server);
}

void			Config::handleLocRoot(std::stringstream &ss, LocationConfig &loc)
{
	ss >> loc.root;
	loc.root = removeSemicolon(loc.root);
}

void			Config::handleLocAutoindex(std::stringstream &ss, LocationConfig &loc)
{
	std::string	val;
	ss >> val;
	val = removeSemicolon(val);
	loc.autoindex = (val == "on");
}

void			Config::handleLocIndex(std::stringstream &ss, LocationConfig &loc)
{
	std::string	val;
	while (ss >> val)
		loc.index.push_back(removeSemicolon(val));
}

void			Config::handleLocAllowMethods(std::stringstream &ss, LocationConfig &loc)
{
	std::string	val;
	while (ss >> val)
		loc.allow_methods.push_back(removeSemicolon(val));
}

void			Config::handleLocReturn(std::stringstream &ss, LocationConfig &loc)
{
	int		code;
	std::string	url;
	ss >> code >> url;
	loc.return_loc = std::make_pair(code, removeSemicolon(url));
}

void			Config::handleLocCgiExt(std::stringstream &ss, LocationConfig &loc)
{
	ss >> loc.cgi_ext;
	loc.cgi_ext = removeSemicolon(loc.cgi_ext);
}

void			Config::handleLocCgiPath(std::stringstream &ss, LocationConfig &loc)
{
	ss >> loc.cgi_path;
	loc.cgi_path = removeSemicolon(loc.cgi_path);
}

void			Config::handleLocUploadStore(std::stringstream &ss, LocationConfig &loc)
{
	ss >> loc.upload_store;
	loc.upload_store = removeSemicolon(loc.upload_store);
}

void			Config::parseLocation(std::ifstream &file, std::string &line, ServerConfig &server)
{
	LocationConfig		loc;
	std::stringstream	header(line);
	std::string		tmp;
	header >> tmp; // skip "location"
	std::string		part;
	while (header >> part && part != "{")
	{
		if (!loc.path.empty()) loc.path += " ";
			loc.path += part;
	}
	static const	std::string keys[] = {"root", "autoindex", "index", "allow_methods", "return", "cgi_ext", "cgi_path", "upload_store", "cgi"};
	static const	LocationDirectiveHandler handlers[] = { &Config::handleLocRoot, &Config::handleLocAutoindex, &Config::handleLocIndex, &Config::handleLocAllowMethods,
								&Config::handleLocReturn, &Config::handleLocCgiExt, &Config::handleLocCgiPath, &Config::handleLocUploadStore,
								&Config::handleLocCgiExt};
	const int	count = sizeof(keys) / sizeof(keys[0]);
	while (std::getline(file, line))
	{
		line = trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		if (line == "}")
			break;
		if (line == "{")
			continue;
		std::stringstream	ss(line);
		std::string		key;
		ss >> key;
		bool			handled = false;
		for (int i = 0; i < count; ++i)
		{
			if (key == keys[i])
			{
				(this->*handlers[i])(ss, loc);
				handled = true;
				break;
			}
		}
		if (!handled)
		throw (std::runtime_error("Unknown location directive: " + key));
	}
	server.locations.push_back(loc);
}
