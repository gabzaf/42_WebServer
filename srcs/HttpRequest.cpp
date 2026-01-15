#include "HttpRequest.hpp"
#include <iostream>
#include <cstdio>
#include <cstdlib>

HttpRequest::HttpRequest(const std::string &raw_request){parse(raw_request);}

HttpRequest::~HttpRequest(){}

std::string	HttpRequest::getMethod() const {return (_method);}

std::string	HttpRequest::getPath() const {return (_path);}
std::string	HttpRequest::getQueryString() const {return (_queryString);}
std::string	HttpRequest::getBody() const {return (_body);}

std::string	HttpRequest::getVersion() const {return (_version);}

std::string	HttpRequest::getHeader(const std::string &key) const
{
	std::map<std::string, std::string>::const_iterator 	it = _headers.find(key);
	if (it != _headers.end())
		return (it->second);
	return ("");
}

void	HttpRequest::parse(const std::string &raw_request)
{
	std::stringstream  	ss(raw_request);
	std::string		line;

	// 1. Request Line: skip any leading blank lines then parse request line
	while (std::getline(ss, line) && (line.empty() || line == "\r"))
		;
	if (!line.empty() && line != "\r")
	{
		std::stringstream	lineStream(line);
		lineStream >> _method >> _path >> _version;

		// Split path and query string
		size_t	queryPos = _path.find('?');
		if (queryPos != std::string::npos)
		{
			_queryString = _path.substr(queryPos + 1);
			_path = _path.substr(0, queryPos);
		}
	}
	// 2. Headers
	while (std::getline(ss, line) && line != "\r")
	{
		if (line.empty())
			break ;
		size_t	colonPos = line.find(':');
		if (colonPos != std::string::npos)
		{
			std::string	key = line.substr(0, colonPos);
			std::string	value = line.substr(colonPos + 1);
			// Trim whitespace
			size_t first = value.find_first_not_of(" \t");
			if (first != std::string::npos)
				value = value.substr(first);
			size_t	last = value.find_last_not_of(" \t\r");
			if (last != std::string::npos)
				value = value.substr(0, last + 1);
			_headers[key] = value;
		}
	}
	// 3. Body: read if Content-Length OR chunked
	std::map<std::string, std::string>::const_iterator itL = _headers.find("Content-Length");
	if (itL != _headers.end()) {
		int contentLength = std::atoi(itL->second.c_str());
		if (contentLength > 0) {
			std::string body;
			body.resize(contentLength);
			ss.read(&body[0], contentLength);
			_body = body;
		}
	} else if (_headers.count("Transfer-Encoding") && _headers["Transfer-Encoding"] == "chunked") {
		std::string chunkLine;
		while (std::getline(ss, chunkLine)) {
			if (chunkLine.empty() || chunkLine == "\r") continue;
			int chunkSize = std::strtol(chunkLine.c_str(), NULL, 16);
			if (chunkSize <= 0) break;
			std::string chunkBody;
			chunkBody.resize(chunkSize);
			ss.read(&chunkBody[0], chunkSize);
			_body += chunkBody;
			std::getline(ss, chunkLine); // skip CRLF after chunk
		}
	}
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const
{
	return (_headers);
}
