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
const std::fstream::pos_type max_size = 100 * 1024 * 1024;
//TODO configurable file directory
constexpr string_view www_dir = "/var/www"sv;

std::pair<std::ifstream, std::size_t> open_file(
	const Request& req, const RhStaticFile::Context& ctx)
{
	const auto path = req.url.path;
	//TODO filesystem::path
	lemon::String fname{ www_dir.begin(), www_dir.end(), ctx.a.make_allocator<char>() };
	fname.append(path.begin(), path.end());

	//TODO consider other IO APIs
	std::ifstream f{ fname.c_str(), std::ios::in | std::ios::binary | std::ios::ate };
	if (!f.is_open())
		throw HttpException{ Response::Status::not_found };
	f.exceptions(std::ios::badbit | std::ios::failbit);

	//TODO size from metadata
	auto size = f.tellg();
	ctx.lg.debug("open file: '"sv, fname, "', size: "sv, size);
	if (size > max_size)
		throw HttpException{ Response::Status::payload_too_large };
	auto mem_length = static_cast<std::size_t>(size);

	return make_pair(move(f), mem_length);
}
}

string_view RhStaticFile::get_name() const noexcept
{
	return "StaticFile"sv;
}

void RhStaticFile::get(Request& req, Response& resp, Context& ctx)
{
	auto [f, length] = open_file(req, ctx);

	f.seekg(0, std::ios::beg);
	auto body_mem = ctx.a.alloc(length, "StaticFile buffer");
	auto buffer = static_cast<char*>(body_mem);
	f.read(buffer, length);

	resp.body.emplace_back(buffer, length);
	finalize(req, resp, ctx, length);
}

void RhStaticFile::head(Request& req, Response& resp, Context& ctx)
{
	auto open_result = open_file(req, ctx);
	auto length = open_result.second;
	finalize(req, resp, ctx, length);
}

void RhStaticFile::finalize(Request& req, Response& resp, Context& ctx, std::size_t length)
{
	resp.http_version = req.http_version;
	//TODO Content-Type
	resp.headers.emplace_back("Content-Length"sv, StringBuilder{ ctx.a }.convert(length));
	resp.code = Response::Status::ok;
}