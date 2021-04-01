#include "testing.hpp"
#include "config.hpp"
#include "http_error.hpp"
#include "http_message.hpp"
#include "http_request_handler.hpp"
#include "logger.hpp"
#include "string_builder.hpp"
#include <boost/assign/std/list.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <functional>

namespace http
{
namespace
{
struct Test : RequestHandler
{
	static auto finalize(Request& req, Response& resp, Context& ctx, string_view type = "text/plain")
	{
		resp.http_version = req.http_version;
		auto content_length = calc_content_length(resp);
		auto content_length_s = StringBuilder{ ctx.a }.convert(content_length);

		resp.headers.insert(resp.headers.end(), {
			{ "Content-Type"sv, type },
			{ "Content-Length"sv, content_length_s },
		});

		resp.code = Response::Status::ok;
	}
};

struct TestIndex : Test
{
	auto get_name() const noexcept -> string_view override { return "test.index"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		resp.body = { "It works!"sv };
		finalize(req, resp, ctx);
	}

	auto method(string_view method_name, Request& req, Response& resp, Context& ctx) -> void override
	{
		if (method_name == "DELETE")
			resp.body = { "del"sv };
		else
			throw Exception{ Response::Status::method_not_allowed };

		finalize(req, resp, ctx);
	}
};
	
struct TestHeaders : Test
{
	auto get_name() const noexcept -> string_view override { return "test.headers"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		for (const auto& hdr : req.headers) {
			using namespace boost::assign;
			resp.body += hdr.name, Message::Header::sep, hdr.value, Message::nl;
		}
		finalize(req, resp, ctx);
	}
};
	
struct TestUa : Test
{
	auto get_name() const noexcept -> string_view override { return "test.ua"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		auto it = boost::find_if(req.headers, Request::Header::make_is("user-agent"sv));
		if (it != req.headers.end())
			resp.body = { it->value };
		finalize(req, resp, ctx);
	}
};

struct TestTitle : Test
{
	TestTitle(std::string title) : title(move(title)) {}
	
	auto get_name() const noexcept -> string_view override { return "test.title"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		resp.body = {
			"<!DOCTYPE html><html><body><h1>",
			title,
			"</h1></body></html>"
		};
		finalize(req, resp, ctx, "text/html");
	}

private:
	const std::string title;
};

struct TestNotImp : Test
{
	auto get_name() const noexcept -> string_view override { return "test.notimp"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		throw Exception{ Response::Status::not_implemented };
	}
};
	
struct TestOom : Test
{
	auto get_name() const noexcept -> string_view override { return "test.oom"; }

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		throw std::bad_alloc{};
	}
};

struct TestEcho : Test
{
	auto get_name() const noexcept -> string_view override { return "test.echo"; }

	auto post(Request& req, Response& resp, Context& ctx) -> void override
	{
		resp.body = move(req.body);
		finalize(req, resp, ctx);
	}
};
}
}

auto ModuleTesting::get_name() const noexcept -> string_view
{
	return "testing"sv;
}

auto ModuleTesting::init(Logger& lg, const config::Table* config) -> HandlerList
{
	std::string title = "default title";
	if (config)
		if (auto& title_prop = (*config)["title"]; title_prop)
			title = title_prop.as<config::String>();
	
	HandlerList handlers = {
		std::make_shared<http::TestIndex>(),
		std::make_shared<http::TestHeaders>(),
		std::make_shared<http::TestUa>(),
		std::make_shared<http::TestTitle>(title),
		std::make_shared<http::TestNotImp>(),
		std::make_shared<http::TestOom>(),
		std::make_shared<http::TestEcho>(),
	};

	return handlers;
}