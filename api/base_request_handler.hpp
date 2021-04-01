#pragma once
#include "string_view.hpp"

struct BaseRequestHandler
{
	virtual ~BaseRequestHandler() = default;

	virtual auto get_name() const noexcept -> string_view = 0;

};