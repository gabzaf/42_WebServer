#include "HttpResponse.hpp"
#include <iostream>
#include <sstream>

HttpResponse::HttpResponse() : _status_code(200), _status_message("OK")
{
	_headers["Server"] = "Webserv/1.0";
}

HttpResponse::~HttpResponse(){}

void	HttpResponse::setStatus(int code, const std::string &message)
{
	_status_code = code;
	_status_message = message;
}

void	HttpResponse::setHeader(const std::string &key, const std::string &value)
{
	_headers[key] = value;
}

void	HttpResponse::setBody(const std::string &body)
{
	_body = body;
	std::stringstream ss;
	ss << _body.length();
	_headers["Content-Length"] = ss.str();
}

std::string HttpResponse::toString() const
{
	std::stringstream ss;
	// Status Line
	ss << "HTTP/1.1 " << _status_code << " " << _status_message << "\r\n";
	// Headers
	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
	{
		ss << it->first << ": " << it->second << "\r\n";
	}
	ss << "\r\n";
	ss << _body;
	return (ss.str());
}
