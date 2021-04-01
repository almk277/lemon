#pragma once
#include "module.hpp"

struct ModuleStaticFile : Module
{
	auto get_name() const noexcept -> string_view override;
	auto init(Logger& lg, const config::Table* config) -> HandlerList override;
	auto description() const noexcept -> string_view override;
};