#pragma once

#include "request_handler.hpp"

struct rh_static_file : request_handler
{
	rh_static_file() = default;

	string_view get_name() const noexcept override;

	void get(request &req, response &resp, context &ctx) override;
	void head(request &req, response &resp, context &ctx) override;

private:
	static void finalize(request &req, response &resp, context &ctx, std::size_t length);
};