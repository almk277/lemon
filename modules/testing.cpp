#include "testing.hpp"
#include "message.hpp"
#include "core/http_error.hpp"
#include "string_builder.hpp"
#include <algorithm>
#include <numeric>

#include "logger.hpp"
boost::string_view rh_testing::get_name() const noexcept
{
	return "Testing"_w;
}

void rh_testing::get(request& req, response& resp, context& ctx)
{
	ctx.lg.debug("Handling get...");

	const auto path = req.url.path;
	if (path == "/index") {
		resp.body = { "It works!"_w };
	} else if (path == "/query") {
		resp.body = { req.url.query };
	} else if (path == "/headers") {
		for (const auto &hdr : req.headers) {
			resp.body.emplace_back(hdr.name);
			resp.body.emplace_back(message::header::SEP);
			resp.body.emplace_back(hdr.value);
			resp.body.emplace_back(message::NL);
		}
	} else if (path == "/ua") {
		auto it = std::find_if(req.headers.begin(), req.headers.end(), [](const auto &hdr)
		{
			//TODO use lcase_name
			return hdr.name == "User-Agent"_w;
		});
		if (it != req.headers.end())
			resp.body = { it->value };
	} else if (path == "/notimp") {
		throw http_exception{ response_status::NOT_IMPLEMENTED };
	} else if (path == "/oom") {
		throw std::bad_alloc{};
	} else {
		throw http_exception{ response_status::NOT_FOUND };
	}

	finalize(req, resp, ctx);
}

void rh_testing::post(request& req, response& resp, context& ctx)
{
	if (req.url.path == "/echo") {
		resp.body = move(req.body);
	} else {
		throw http_exception{ response_status::NOT_FOUND };
	}

	finalize(req, resp, ctx);
}

void rh_testing::method(boost::string_view method_name, request &req, response &resp, context &ctx)
{
	if (method_name == "DELETE") {
		if (req.url.path == "/index") {
			resp.body = { "del"_w };
		} else {
			throw http_exception{ response_status::NOT_FOUND };
		}
	} else {
		throw http_exception{ response_status::METHOD_NOT_ALLOWED };
	}

	finalize(req, resp, ctx);
}

void rh_testing::finalize(request& req, response& resp, context& ctx) const
{
	resp.http_version = req.http_version;
	resp.code = response_status::OK;
	resp.headers.emplace_back("Content-Type", "text/plain");

	auto content_length = accumulate(resp.body.begin(), resp.body.end(),
		0, [](auto init, const auto &chunk)
		{
			return init + chunk.size();
		});
	resp.headers.emplace_back("Content-Length",
		string_builder{ ctx.a }.convert(content_length));
}
