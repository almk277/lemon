#pragma once
#include "string_view.hpp"

struct Request;
struct Response;
class Arena;
class Logger;

struct RequestHandler
{
	struct Context
	{
		Arena &a;
		Logger &lg;
	};

	RequestHandler() = default;
	RequestHandler(const RequestHandler&) = delete;
	RequestHandler &operator=(const RequestHandler&) = delete;
	RequestHandler(RequestHandler&&) = delete;
	RequestHandler &operator=(RequestHandler&&) = delete;
	virtual ~RequestHandler() = default;

	virtual string_view get_name() const noexcept = 0;

	virtual void get(Request &req, Response &resp, Context &ctx);
	virtual void head(Request &req, Response &resp, Context &ctx);
	virtual void post(Request &req, Response &resp, Context &ctx);
	virtual void method(string_view method_name, Request &req,
		Response &resp, Context &ctx);
};