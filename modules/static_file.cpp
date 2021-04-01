#include "static_file.hpp"
#include "arena.hpp"
#include "config.hpp"
#include "http_error.hpp"
#include "http_message.hpp"
#include "http_request_handler.hpp"
#include "logger.hpp"
#include "string.hpp"
#include "string_builder.hpp"
#include "string_view.hpp"
#include <fstream>

//TODO chunked send
//TODO caching
//TODO Linux sendfile?

namespace http
{
namespace
{
const std::fstream::pos_type max_size = 100 * 1024 * 1024;

struct RhStaticFile : RequestHandler
{
	explicit RhStaticFile(std::string root)
		: www_dir{move(root)}
	{}
	
	auto get_name() const noexcept -> string_view override
	{
		return "static"sv;
	}

	auto get(Request& req, Response& resp, Context& ctx) -> void override
	{
		auto [f, length] = open_file(req, ctx);

		f.seekg(0, std::ios::beg);
		auto body_mem = ctx.a.alloc(length, "StaticFile buffer");
		auto buffer = static_cast<char*>(body_mem);
		f.read(buffer, length);

		resp.body.emplace_back(buffer, length);
		finalize(req, resp, ctx, length);
	}
	
	auto head(Request& req, Response& resp, Context& ctx) -> void override
	{
		auto length = open_file(req, ctx).second;
		finalize(req, resp, ctx, length);
	}

private:
	auto open_file(const Request& req, const Context& ctx) const -> std::pair<std::ifstream, std::size_t>
	{
		const auto path = req.url.path;
		lemon::String fname{ www_dir.begin(), www_dir.end(), ctx.a.make_allocator<char>() };
		fname.append(path.begin(), path.end());

		//TODO consider other IO APIs
		std::ifstream f{ fname.c_str(), std::ios::in | std::ios::binary | std::ios::ate };
		if (!f.is_open())
			throw Exception{ Response::Status::not_found };
		f.exceptions(std::ios::badbit | std::ios::failbit);

		//TODO size from metadata
		auto size = f.tellg();
		ctx.lg.debug("open file: '"sv, fname, "', size: "sv, size);
		if (size > max_size)
			throw Exception{ Response::Status::payload_too_large };
		auto mem_length = static_cast<std::size_t>(size);

		return { move(f), mem_length };
	}

	static auto finalize(Request& req, Response& resp, Context& ctx, std::size_t length) -> void
	{
		resp.http_version = req.http_version;
		//TODO Content-Type
		resp.headers.emplace_back("Content-Length"sv, StringBuilder{ ctx.a }.convert(length));
		resp.code = Response::Status::ok;
	}
	
	//TODO server-specific root
	const std::string www_dir;
};
}
}

auto ModuleStaticFile::get_name() const noexcept -> string_view
{
	return "static_file"sv;
}

auto ModuleStaticFile::init(Logger& lg, const config::Table* config) -> HandlerList
{
	if (!config)
		throw std::runtime_error{ "config is mandatory for this module" };
	auto& root = (*config)["root"].as<config::String>();
	lg.debug("root directory: ", root);
	
	HandlerList handlers = {
		std::make_shared<http::RhStaticFile>(root),
	};

	return handlers;
}

auto ModuleStaticFile::description() const noexcept -> string_view
{
	return "Handles requests for static files"sv;
}