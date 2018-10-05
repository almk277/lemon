#include "string_builder.hpp"
#include <boost/lexical_cast.hpp>
#include <limits>
#include <array>
#include <cstring>

string_view string_builder::convert(std::size_t n)
{
	constexpr int s_len = std::numeric_limits<std::size_t>::digits10
		+ 1 // not accounted by digits10
		+ 1; // null-byte
	//TODO to_chars
	auto buffer = boost::lexical_cast<std::array<char, s_len>>(n);
	auto buffer_len = std::strlen(buffer.data());
	auto result = allocator.allocate(buffer_len);
	std::memcpy(result, buffer.data(), buffer_len);
	return {result, buffer_len};
}
