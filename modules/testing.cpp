#include "testing.hpp"
#include "message.hpp"
#include "http_error.hpp"
#include "string_builder.hpp"
#include "logger.hpp"
#include <boost/range/algorithm/find_if.hpp>
#include <boost/assign/std/list.hpp>
#include <functional>

string_view RhTesting::get_name() const noexcept
{
	return "Testing"sv;
}

void RhTesting::get(Request &req, Response &resp, Context &ctx)
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
			resp.body += hdr.name, Message::Header::sep, hdr.value, Message::nl;
		}
	} else if (path == "/ua") {
		auto it = boost::find_if(req.headers, Request::Header::make_is("user-agent"sv));
		if (it != req.headers.end())
			resp.body = { it->value };
	} else if (path == "/notimp") {
		throw HttpException{ Response::Status::not_implemented };
	} else if (path == "/oom") {
		throw std::bad_alloc{};
	} else {
		throw HttpException{ Response::Status::not_found };
	}

	finalize(req, resp, ctx);
}

void RhTesting::post(Request &req, Response &resp, Context &ctx)
{
	if (req.url.path == "/echo") {
		resp.body = move(req.body);
	} else {
		throw HttpException{ Response::Status::not_found };
	}

	finalize(req, resp, ctx);
}

void RhTesting::method(string_view method_name, Request &req, Response &resp, Context &ctx)
{
	if (method_name == "DELETE") {
		if (req.url.path == "/index") {
			resp.body = { "del"sv };
		} else {
			throw HttpException{ Response::Status::not_found };
		}
	} else {
		throw HttpException{ Response::Status::method_not_allowed };
	}

	finalize(req, resp, ctx);
}

void RhTesting::finalize(Request &req, Response &resp, Context &ctx)
{
	resp.http_version = req.http_version;
	auto content_length = calc_content_length(resp);
	auto content_length_s = StringBuilder{ ctx.a }.convert(content_length);

	resp.headers.insert(resp.headers.end(), {
		{ "Content-Type"sv, "text/plain"sv },
		{ "Content-Length"sv, content_length_s },
	});

	resp.code = Response::Status::ok;
}