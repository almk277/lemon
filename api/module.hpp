#pragma once
#include "string_view.hpp"
#include <memory>
#include <vector>

struct BaseRequestHandler;
class Logger;

namespace config
{
class Table;
}

struct Module
{
	using HandlerList = std::vector<std::shared_ptr<BaseRequestHandler>>;
	
	virtual ~Module() = default;

	virtual auto get_name() const noexcept -> string_view = 0;
	virtual auto init(Logger& lg, const config::Table* config) -> HandlerList = 0;
	virtual auto description() const noexcept -> string_view;
};

inline auto Module::description() const noexcept -> string_view
{
	return {};
}