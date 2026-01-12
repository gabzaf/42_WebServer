#pragma once

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <vector>

class	HttpRequest
{
	private:
		std::string				_method;
		std::string				_path;
		std::string				_queryString;
		std::string				_version;
		std::map<std::string, std::string>	_headers;
		std::string				_body;

		void					parse(const std::string &raw_request);

	public:
		HttpRequest(const std::string &raw_request);
		~HttpRequest();

		std::string				getMethod() const;
		std::string				getPath() const;
		std::string				getQueryString() const;
		std::string				getHeader(const std::string &key) const;
		std::string				getBody() const;
		std::string				getVersion() const;
		const std::map<std::string, std::string>& getHeaders() const;
};
