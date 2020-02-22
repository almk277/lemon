#pragma once

#include "string_view.hpp"
struct request;
struct response;
class arena;
class logger;

struct request_handler
{
	struct context
	{
		arena &a;
		logger &lg;
	};

	request_handler() = default;
	request_handler(const request_handler&) = delete;
	request_handler &operator=(const request_handler&) = delete;
	request_handler(request_handler&&) = delete;
	request_handler &operator=(request_handler&&) = delete;
	virtual ~request_handler() = default;

	virtual string_view get_name() const noexcept = 0;

	virtual void get(request &req, response &resp, context &ctx);
	virtual void head(request &req, response &resp, context &ctx);
	virtual void post(request &req, response &resp, context &ctx);
	virtual void method(string_view method_name, request &req,
		response &resp, context &ctx);
};