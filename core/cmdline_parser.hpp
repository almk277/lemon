#pragma once
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <string>
#include <ostream>

struct Parameters;

class CommandLine
{
public:
	auto has(const std::string &parameter) const noexcept -> bool;
	auto to_parameters() const -> Parameters;

private:
	boost::program_options::variables_map vars;
	friend class CommandLineParser;
};

class CommandLineParser
{
public:
	CommandLineParser();
	// throws std::logic_error
	auto parse(int argc, const char *const argv[]) const -> CommandLine;
	auto print_options(std::ostream &stream) const -> void;

private:
	const boost::program_options::options_description desc;
};