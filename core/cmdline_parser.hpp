#pragma once
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <string>
#include <ostream>

struct parameters;

class command_line
{
public:
	auto has(const std::string &parameter) const noexcept -> bool;
	auto to_parameters() const -> parameters;

private:
	boost::program_options::variables_map vars;
	friend class command_line_parser;
};

class command_line_parser
{
public:
	command_line_parser();
	// throws std::logic_error
	auto parse(int argc, const char *const argv[]) const -> command_line;
	auto print_options(std::ostream &stream) const -> void;

private:
	const boost::program_options::options_description desc;
};
