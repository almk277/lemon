#include "static_file.hpp"
#include "message.hpp"
#include "logger.hpp"
#include "arena.hpp"
#include "http_error.hpp"
#include "string_builder.hpp"
#include "string.hpp"
#include "string_view.hpp"
#include <fstream>

//TODO chunked send
//TODO caching
//TODO Linux sendfile?

namespace
{
const std::fstream::pos_type MAX_SIZE = 100 * 1024 * 1024;
//TODO configurable file directory
constexpr string_view WWW_DIR = "/var/www"sv;

std::pair<std::ifstream, std::size_t> open_file(
	const request &req, const rh_static_file::context &ctx)
{
	const auto path = req.url.path;
	//TODO filesystem::path
	lemon::string fname{ WWW_DIR.begin(), WWW_DIR.end(), ctx.a.make_allocator<char>() };
	fname.append(path.begin(), path.end());

	//TODO consider other IO APIs
	std::ifstream f{ fname.c_str(), std::ios::in | std::ios::binary | std::ios::ate };
	if (!f.is_open())
		throw http_exception{ response::status::NOT_FOUND };
	f.exceptions(std::ios::badbit | std::ios::failbit);

	//TODO size from metadata
	auto size = f.tellg();
	ctx.lg.debug("open file: '"sv, fname, "', size: "sv, size);
	if (size > MAX_SIZE)
		throw http_exception{ response::status::PAYLOAD_TOO_LARGE };
	auto mem_length = static_cast<std::size_t>(size);

	return make_pair(move(f), mem_length);
}
}

string_view rh_static_file::get_name() const noexcept
{
	return "StaticFile"sv;
}

void rh_static_file::get(request &req, response &resp, context &ctx)
{
	auto open_result = open_file(req, ctx);
	auto f = move(open_result.first);
	auto length = open_result.second;

	f.seekg(0, std::ios::beg);
	auto body_mem = ctx.a.alloc(length, "StaticFile buffer");
	auto buffer = static_cast<char*>(body_mem);
	f.read(buffer, length);

	resp.body.emplace_back(buffer, length);
	finalize(req, resp, ctx, length);
}

void rh_static_file::head(request &req, response &resp, context &ctx)
{
	auto open_result = open_file(req, ctx);
	auto length = open_result.second;
	finalize(req, resp, ctx, length);
}

void rh_static_file::finalize(request &req, response &resp, context &ctx, std::size_t length)
{
	resp.http_version = req.http_version;
	//TODO Content-Type
	resp.headers.emplace_back("Content-Length"sv, string_builder{ ctx.a }.convert(length));
	resp.code = response::status::OK;
}