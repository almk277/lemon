#pragma once
#include "arena.hpp"
#include "string_view.hpp"

class StringBuilder
{
public:
	explicit constexpr StringBuilder(Arena& a) noexcept:
		allocator{a}
	{}

	string_view convert(std::size_t n);

private:
	Arena::Allocator<char> allocator;
};