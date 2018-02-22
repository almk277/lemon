#include "hello.hpp"
#include "message.hpp"
#include "logger.hpp"

boost::string_view rh_hello::get_name() const noexcept
{
	return "Hello"_w;
}

void rh_hello::get(request &req, response &resp, context &ctx)
{
	resp.http_version = req.http_version;
	resp.code = response_status::OK;
	resp.headers.emplace_back("Content-Type", "text/plain");
	resp.headers.emplace_back("Content-Length", "9");
	resp.body.emplace_back("It works!"_w);
	ctx.lg.info("sending hello...");
}
