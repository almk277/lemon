#pragma once
#include "config.hpp"
#include <iostream>

namespace config
{
using value = table::value;
using int_ = value::int_;
using real = value::real;
using std::string;
}

auto operator<<(std::ostream &stream, const table::value &v) -> std::ostream&;

auto operator<<(std::ostream &stream, const table &t) -> std::ostream&;