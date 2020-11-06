#include "cmdline_parser.hpp"
#include "parameters.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <vector>
#include <string>
#include <iterator>
#include <stdexcept>

namespace
{
using CmdlineArgs = std::vector<std::string>;

struct CmdlineFixture
{
	CommandLineParser parser;

	auto cmdline(const CmdlineArgs &args) const
	{
		std::vector<const char*> argv = { "test-program" };
		copy(args | boost::adaptors::transformed(mem_fn(&std::string::c_str)), back_inserter(argv));

		auto argc = static_cast<int>(argv.size());
		return parser.parse(argc, argv.data());
	}
	auto to_parameters(const CmdlineArgs &args) const
	{
		return cmdline(args).to_parameters();
	}
};
}

#define ERROR_UNKNOWN(expr) BOOST_CHECK_THROW((expr), std::logic_error)
#define ERROR_BAD_SYNTAX(expr) BOOST_CHECK_THROW((expr), std::logic_error)
#define ERROR_MULTIPLE(expr) BOOST_CHECK_THROW((expr), std::logic_error)

BOOST_FIXTURE_TEST_SUITE(cmdline_parser_tests, CmdlineFixture)

BOOST_AUTO_TEST_CASE(help)
{
	BOOST_TEST(cmdline({ "-h" }).has("help"));
	BOOST_TEST(cmdline({ "--help" }).has("help"));
	BOOST_TEST(!cmdline({}).has("help"));
	ERROR_UNKNOWN(cmdline({ "-H" }));
	ERROR_UNKNOWN(cmdline({ "-he" }));
	ERROR_UNKNOWN(cmdline({ "-help" }));
	ERROR_UNKNOWN(cmdline({ "--hel" }));
	ERROR_BAD_SYNTAX(cmdline({ "--help=true" }));
}

BOOST_AUTO_TEST_CASE(version)
{
	BOOST_TEST(cmdline({ "-v" }).has("version"));
	BOOST_TEST(cmdline({ "--version" }).has("version"));
	BOOST_TEST(!cmdline({}).has("version"));
	ERROR_UNKNOWN(cmdline({ "-V" }));
	ERROR_UNKNOWN(cmdline({ "-ve" }));
	ERROR_UNKNOWN(cmdline({ "-version" }));
	ERROR_UNKNOWN(cmdline({ "--ver" }));
	ERROR_BAD_SYNTAX(cmdline({ "--version=1" }));
}

BOOST_AUTO_TEST_CASE(config)
{
	BOOST_TEST(to_parameters({ "-cpath/to/config" }).config_path == "path/to/config");
	BOOST_TEST(to_parameters({ "-c", "path/to/config" }).config_path == "path/to/config");
	BOOST_TEST(to_parameters({ "--config=path/to/config" }).config_path == "path/to/config");
	BOOST_TEST(to_parameters({ "--config", "path/to/config" }).config_path == "path/to/config");
	ERROR_BAD_SYNTAX(cmdline({ "-c" }));
	ERROR_BAD_SYNTAX(cmdline({ "--config" }));
	ERROR_MULTIPLE(cmdline({ "-cpath1", "-cpath2" }));
}

BOOST_AUTO_TEST_SUITE_END()