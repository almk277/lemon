#pragma once

#include "request_handler.hpp"

struct rh_hello: request_handler
{
	rh_hello() = default;
	~rh_hello() = default;

	boost::string_view get_name() const noexcept override;

	void get(request &req, response &resp, context &ctx) override;
};
