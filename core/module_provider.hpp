#pragma once
#include "string_view.hpp"
#include <memory>
#include <vector>

struct Module;

struct ModuleProvider
{
	using List = std::vector<std::unique_ptr<Module>>;
	
	virtual ~ModuleProvider() = default;

	virtual auto get_name() const noexcept -> string_view = 0;
	virtual auto get() -> List = 0;
};

struct BuiltinModules : ModuleProvider
{
	auto get_name() const noexcept -> string_view override { return "builtin modules"; }
	auto get() -> List override;
};