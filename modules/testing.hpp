#pragma once
#include "http_request_handler.hpp"

namespace http
{
struct RhTesting : RequestHandler
{
	RhTesting() = default;

	string_view get_name() const noexcept override;

	void get(Request& req, Response& resp, Context& ctx) override;
	void post(Request& req, Response& resp, Context& ctx) override;
	void method(string_view method_name, Request&, Response&, Context&) override;

private:
	static void finalize(Request& req, Response& resp, Context& ctx);
};
}