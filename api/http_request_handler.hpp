#pragma once
#include "base_request_handler.hpp"
#include "string_view.hpp"

class Arena;
class Logger;

namespace http
{
struct Request;
struct Response;

struct RequestHandler : BaseRequestHandler
{
	struct Context
	{
		Arena& a;
		Logger& lg;
	};

	RequestHandler() = default;
	RequestHandler(const RequestHandler&) = delete;
	RequestHandler& operator=(const RequestHandler&) = delete;
	RequestHandler(RequestHandler&&) = delete;
	RequestHandler& operator=(RequestHandler&&) = delete;
	virtual ~RequestHandler() = default;

	virtual auto get(Request& req, Response& resp, Context& ctx) -> void;
	virtual auto head(Request& req, Response& resp, Context& ctx) -> void;
	virtual auto post(Request& req, Response& resp, Context& ctx) -> void;
	virtual auto method(string_view method_name, Request& req,
	                    Response& resp, Context& ctx) -> void;
};
}