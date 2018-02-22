#pragma once

struct request;
struct response;
class arena;
class logger;
#include <boost/utility/string_view.hpp>

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

	virtual boost::string_view get_name() const noexcept = 0;

	virtual void get(request &req, response &resp, context &ctx);
	virtual void head(request &req, response &resp, context &ctx);
	virtual void post(request &req, response &resp, context &ctx);
	virtual void method(boost::string_view method_name, request &req,
		response &resp, context &ctx);
};