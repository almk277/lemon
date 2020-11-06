#include "cmdline_parser.hpp"
#include "parameters.hpp"
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/cmdline.hpp>

#ifndef LEMON_CONFIG_PATH
# define LEMON_CONFIG_PATH ./lemon.ini
#endif

namespace
{
auto make_desc()
{
	boost::program_options::options_description desc{ "Lemon is a web-server. Options are:" };
	desc.add_options()
		("config,c", 
			boost::program_options::value<std::string>()
				->value_name("path")
				->default_value(BOOST_STRINGIZE(LEMON_CONFIG_PATH)),
			"config file path")
		("help,h", "print help and exit")
		("version,v", "print version and exit");

	return desc;
}
}

CommandLineParser::CommandLineParser():
	desc{ make_desc() }
{
}

auto CommandLineParser::parse(int argc, const char *const argv[]) const -> CommandLine
{
	namespace style = boost::program_options::command_line_style;

	CommandLine result;
	auto options = parse_command_line(argc, argv, desc,
		style::default_style & ~style::allow_guessing);
	store(options, result.vars);
	notify(result.vars);

	return result;
}

auto CommandLineParser::print_options(std::ostream &stream) const -> void
{
	stream << desc;
}

auto CommandLine::has(const std::string &parameter) const noexcept -> bool
{
	return vars.count(parameter) > 0;
}

auto CommandLine::to_parameters() const -> Parameters
{
	Parameters p;
	p.config_path = vars["config"].as<std::string>();

	return p;
}