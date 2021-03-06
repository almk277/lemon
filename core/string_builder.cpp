#include "string_builder.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

string_view StringBuilder::convert(std::size_t n)
{
	constexpr int s_len = std::numeric_limits<std::size_t>::digits10
		+ 1; // not accounted by digits10

	std::array<char, s_len> temp;
	auto [end, err] = std::to_chars(temp.data(), temp.data() + temp.size(), n);
	if (err != decltype(err){})
		throw std::runtime_error{ "can't convert number to string: " + std::to_string(n) };
	auto len = end - temp.data();
	auto mem = allocator.allocate(len);
	std::memcpy(mem, temp.data(), len);
	return { mem, static_cast<string_view::size_type>(len) };
}