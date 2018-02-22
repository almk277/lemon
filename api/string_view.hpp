#pragma once

#include <boost/utility/string_view.hpp>

using boost::string_view;

constexpr string_view operator ""_w(const char *s, std::size_t n) noexcept
{
	return { s, n };
}