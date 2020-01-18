#pragma once

#include "string_view.hpp"
#include "arena.hpp"

class string_builder
{
public:
	explicit string_builder(arena &a) noexcept:
		allocator{a}
	{}

	string_view convert(std::size_t n);

private:
	arena::allocator<char> allocator;
};
