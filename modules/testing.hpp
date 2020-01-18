#pragma once

#include "request_handler.hpp"

struct rh_testing : request_handler
{
	rh_testing() = default;

	string_view get_name() const noexcept override;

	void get(request &req, response &resp, context &ctx) override;
	void post(request &req, response &resp, context &ctx) override;
	void method(string_view method_name, request&, response&, context&) override;

private:
	static void finalize(request &req, response &resp, context &ctx);
};