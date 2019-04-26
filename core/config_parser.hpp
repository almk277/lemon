#pragma once
#include "utility.hpp"
#include <string>
#include <stdexcept>
#include <cstddef>

class table;

namespace config
{
class error: public std::runtime_error
{
public:
	error(std::size_t position, const std::string& msg);

	std::size_t position = 0;
};

table parse(string_view text);

table parse_file(const std::string& filename);
}
