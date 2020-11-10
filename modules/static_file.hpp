#pragma once
#include "http_request_handler.hpp"

namespace http
{
struct RhStaticFile : RequestHandler
{
	RhStaticFile() = default;

	string_view get_name() const noexcept override;

	void get(Request& req, Response& resp, Context& ctx) override;
	void head(Request& req, Response& resp, Context& ctx) override;

private:
	static void finalize(Request& req, Response& resp, Context& ctx, std::size_t length);
};
}