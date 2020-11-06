#pragma once

#include "arena.hpp"
#include <string>

namespace lemon
{
template<typename CharT, typename Traits = std::char_traits<CharT>>
using BasicString = std::basic_string<CharT, Traits, Arena::Allocator<CharT>>;

using String = BasicString<char>;
using WString = BasicString<wchar_t>;
}