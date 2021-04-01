#pragma once
#include "module.hpp"

struct ModuleTesting : Module
{
	auto get_name() const noexcept -> string_view override;
	auto init(Logger& lg, const config::Table* config) -> HandlerList override;
};