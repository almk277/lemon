#include "testing.hpp"
#include "message.hpp"
#include "http_error.hpp"
#include "string_builder.hpp"
#include "logger.hpp"
#include <boost/range/algorithm/find_if.hpp>
#include <boost/assign/std/list.hpp>
#include <functional>

string_view rh_testing::get_name() const noexcept
{
	return "Testing"sv;
}

void rh_testing::get(request &req, response &resp, context &ctx)
{
	ctx.lg.debug("Handling get...");

	const auto path = req.url.path;
	if (path == "/index") {
		resp.body = { "It works!"sv };
	} else if (path == "/query") {
		resp.body = { req.url.query };
	} else if (path == "/headers") {
		for (const auto &hdr : req.headers) {
			using namespace boost::assign;
			resp.body += hdr.name, message::header::SEP, hdr.value, message::NL;
		}
	} else if (path == "/ua") {
		auto it = boost::find_if(req.headers, request::header::make_is("user-agent"sv));
		if (it != req.headers.end())
			resp.body = { it->value };
	} else if (path == "/notimp") {
		throw http_exception{ response::status::NOT_IMPLEMENTED };
	} else if (path == "/oom") {
		throw std::bad_alloc{};
	} else {
		throw http_exception{ response::status::NOT_FOUND };
	}

	finalize(req, resp, ctx);
}

void rh_testing::post(request &req, response &resp, context &ctx)
{
	if (req.url.path == "/echo") {
		resp.body = move(req.body);
	} else {
		throw http_exception{ response::status::NOT_FOUND };
	}

	finalize(req, resp, ctx);
}

void rh_testing::method(string_view method_name, request &req, response &resp, context &ctx)
{
	if (method_name == "DELETE") {
		if (req.url.path == "/index") {
			resp.body = { "del"sv };
		} else {
			throw http_exception{ response::status::NOT_FOUND };
		}
	} else {
		throw http_exception{ response::status::METHOD_NOT_ALLOWED };
	}

	finalize(req, resp, ctx);
}

void rh_testing::finalize(request &req, response &resp, context &ctx)
{
	resp.http_version = req.http_version;
	auto content_length = calc_content_length(resp);
	auto content_length_s = string_builder{ ctx.a }.convert(content_length);

	resp.headers.insert(resp.headers.end(), {
		{ "Content-Type"sv, "text/plain"sv },
		{ "Content-Length"sv, content_length_s },
	});

	resp.code = response::status::OK;
}
