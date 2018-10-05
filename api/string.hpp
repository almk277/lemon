#pragma once

#include "arena.hpp"
#include <string>

namespace lemon
{
template<typename CharT, typename Traits = std::char_traits<CharT>>
using basic_string = std::basic_string<CharT, Traits, arena::allocator<CharT>>;

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
}