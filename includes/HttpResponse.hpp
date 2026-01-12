#pragma once

#include <string>
#include <map>
#include <sstream>

class	HttpResponse
{
	private:
		int					_status_code;
		std::string				_status_message;
		std::map<std::string, std::string>	_headers;
		std::string				_body;

	public:
		HttpResponse();
		~HttpResponse();

		void					setStatus(int code, const std::string &message);
		void					setHeader(const std::string &key, const std::string &value);
		void					setBody(const std::string &body);
		std::string				toString() const;
};
