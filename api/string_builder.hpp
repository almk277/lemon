#pragma once

#include "utility.hpp"
#include "arena.hpp"
#include <boost/lexical_cast.hpp>
#include <limits>
#include <array>
#include <cstring>

class string_builder
{
public:
	explicit string_builder(arena &a) noexcept:
		allocator{a}
	{}

	string_view convert(std::size_t n)
	{
		constexpr int s_len = std::numeric_limits<std::size_t>::digits10
			+ 1  // not accounted by digits10
			+ 1; // null-byte
		auto buffer = boost::lexical_cast<std::array<char, s_len>>(n);
		auto buffer_len = std::strlen(buffer.data());
		auto result = allocator.allocate(buffer_len);
		std::memcpy(result, buffer.data(), buffer_len);
		return{ result, buffer_len };
	}

private:
	arena::allocator<char> allocator;
};
