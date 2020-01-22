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

command_line_parser::command_line_parser():
	desc{ make_desc() }
{
}

auto command_line_parser::parse(int argc, const char *const argv[]) const -> command_line
{
	namespace style = boost::program_options::command_line_style;

	command_line result;
	auto options = parse_command_line(argc, argv, desc,
		style::default_style & ~style::allow_guessing);
	store(options, result.vars);
	notify(result.vars);

	return result;
}

auto command_line_parser::print_options(std::ostream &stream) const -> void
{
	stream << desc;
}

auto command_line::has(const std::string &parameter) const noexcept -> bool
{
	return vars.count(parameter) > 0;
}

auto command_line::to_parameters() const -> parameters
{
	parameters p;
	p.config_path = vars["config"].as<std::string>();

	return p;
}
