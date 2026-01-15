#pragma once

#include "HttpRequest.hpp"
#include "Config.hpp"

#include <string>
#include <map>
#include <vector>

class	CgiHandler
{
	private:
		const HttpRequest			&_request;
		std::string				_scriptPath;
		std::string				_interpreterPath;
		std::string				_pathInfo;
		std::map<std::string, std::string>	_env;
		
		void					setupEnv();
		char					**getEnvAsCstrArray() const;
		void					freeEnvCstrArray(char **envp) const;
	
	public:
		CgiHandler(const HttpRequest &request, const std::string &scriptPath, const std::string &interpreterPath, const std::string &pathInfo);
		~CgiHandler();

		std::string	execute();
};
