#include "config_parser.hpp"
#include "test_config.hpp"
#include "algorithm.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <initializer_list>
#include <utility>
#include <array>

using namespace config::test;
using config::Table;
using boost::unit_test::data::make;

struct BadSample
{
	string_view text;
	std::size_t error_pos;
	std::string error_msg;
};

using SampleList = std::initializer_list<std::pair<string_view, std::shared_ptr<Table>>>;
using BadSampleList = std::initializer_list<BadSample>;

namespace config
{
static std::ostream &operator<<(std::ostream &stream, SampleList::const_reference sample)
{
	return stream << "TEXT{\"" << sample.first << "\"} -> " << *sample.second;
}
}

static std::ostream &operator<<(std::ostream &stream, BadSampleList::const_reference sample)
{
	return stream << "TEXT{\"" << sample.text << "\"} -> error at "
		<< sample.error_pos << ": " << sample.error_msg;
}

namespace
{
auto tbl()
{
	return std::make_shared<Table>(std::make_unique<TableErrorHandler>());
}

std::string expect(string_view what)
{
	static const std::array<string_view, 3> rule_names = {
		"value",
		"statement",
		"table",
	};

	auto what_s = contains(rule_names, what)
		? std::string{ what }
		: "'" + std::string{ what } + "'";
	return std::string{ "Expecting " } + what_s;
}

SampleList basic_samples = {
	{
		"", tbl()
	},
	{
		" \t\n\t ", tbl()
	},
	{
		"  k  =  true  ",
		tbl() << prop("k", true)
	},
	{
		"\tk\t=\ttrue\t",
		tbl() << prop("k", true)
	},
	{
		"\nk\n=\ntrue\n",
		tbl() << prop("k", true)
	},
	{
		" \t \nk\t \n =\t \n true\t \n ",
		tbl() << prop("k", true)
	},
};

const BadSampleList bad_basic_samples = {
	{ "abc",         3,  expect("=") },
	{ "a b",         2,  expect("=") },
	{ "a 0",         2,  expect("=") },
	{ "a =",         3,  expect("value") },
	{ "a = b c",     7,  expect("=") },
	{ "a = b c =",   9,  expect("value") },
	{ "a = {",       5,  expect("}") },
	{ "a = { b",     7,  expect("=") },
	{ "a = { b =",   9,  expect("value") },
	{ "a = { b = c", 11, expect("}") },
};

const SampleList key_samples = {
	{
		"snake_key_name = 1",
		tbl() << prop("snake_key_name", 1)
	},
	{
		"CamelNameKey = 1",
		tbl() << prop("CamelNameKey", 1)
	},
	{
		"key.dot.x = 1",
		tbl() << prop("key.dot.x", 1)
	},
	{
		"key0123 = 1",
		tbl() << prop("key0123", 1)
	},
	{
		"Real_LONG_012.Key9 = 1",
		tbl() << prop("Real_LONG_012.Key9", 1)
	},
	{
		"trueKey = 1",
		tbl() << prop("trueKey", 1)
	},
};

const SampleList bool_samples = {
	{
		"key = true",
		tbl() << prop("key", true)
	},
	{
		"key = false",
		tbl() << prop("key", false)
	},
	{
		"key = true ",
		tbl() << prop("key", true)
	},
};

const SampleList int_samples = {
	{
		"key = 0",
		tbl() << prop("key", 0)
	},
	{
		"key = -2147483647",
		tbl() << prop("key", -2147483647)
	},
	{
		"key = +2147483647",
		tbl() << prop("key", +2147483647)
	},
	{
		"key = 2147483647",
		tbl() << prop("key", 2147483647)
	},
};

const SampleList real_samples = {
	{
		"key = 0.0",
		tbl() << prop("key", 0.0)
	},
	{
		"key = 12345.6789",
		tbl() << prop("key", 12345.6789)
	},
	{
		"key = -12345.6789",
		tbl() << prop("key", -12345.6789)
	},
	{
		"key = -1.12345e-38",
		tbl() << prop("key", -1.12345e-38)
	},
	{
		"key = -1.12345e+38",
		tbl() << prop("key", -1.12345e+38)
	},
	{
		"key = +1.12345e-38",
		tbl() << prop("key", +1.12345e-38)
	},
	{
		"key = +1.12345e+38",
		tbl() << prop("key", +1.12345e+38)
	},
	{
		"key = 1.12345e-38",
		tbl() << prop("key", 1.12345e-38)
	},
	{
		"key = 1.12345e+38",
		tbl() << prop("key", 1.12345e+38)
	},
};

const SampleList string_samples = {
	{
		"key = value",
		tbl() << prop("key", "value")
	},
	{
		"key = truestring",
		tbl() << prop("key", "truestring")
	},
	{
		"key = True",
		tbl() << prop("key", "True")
	},
	{
		"key = a0123456789",
		tbl() << prop("key", "a0123456789")
	},
	{
		R"(key = a/\._-z)",
		tbl() << prop("key", R"(a/\._-z)")
	},
	{
		"key = 'value'",
		tbl() << prop("key", "value")
	},
	{
		"key = ''",
		tbl() << prop("key", "")
	},
	{
		"key = ' x '",
		tbl() << prop("key", " x ")
	},
	{
		"key = 'a b\tc'",
		tbl() << prop("key", "a b\tc")
	},
	{
		"key = 'true'",
		tbl() << prop("key", "true")
	},
	{
		"key = '1'",
		tbl() << prop("key", "1")
	},
	{
		"key = '0.1'",
		tbl() << prop("key", "0.1")
	},
	{
		"key = '{ x = 0 }'",
		tbl() << prop("key", "{ x = 0 }")
	},
};

const SampleList table_samples = {
	{
		"k = { }",
		tbl() << prop("k", tbl())
	},
	{
		"k = { x = 1 }",
		tbl() << prop("k", tbl() << prop("x", 1))
	},
	{
		"k = {\n\tx = 1\n}",
		tbl() << prop("k", tbl() << prop("x", 1))
	},
	{
		"k = \n{\n  x = 1\n}",
		tbl() << prop("k", tbl() << prop("x", 1))
	},
	{
		"k = { x = 1 y = 2 }",
		tbl() << prop("k", tbl()
			 << prop("x", 1)
			 << prop("y", 2)
		)
	},
};

const SampleList plain_config_samples = {
	{
		"key1 = true key2 = 42 key3 = 3.14 key4 = some_string",
		tbl()
			<< prop("key1", true)
			<< prop("key2", 42)
			<< prop("key3", 3.14)
			<< prop("key4", "some_string")
	},
	{
		R"(
		key1 = true
		key2 = 42

		key3 = 3.14


		key4 = some_string
		)",
		tbl()
			 << prop("key1", true)
			 << prop("key2", 42)
			<< prop("key3", 3.14)
			<< prop("key4", "some_string")
	},
	{
		"k1 = { x = 1 y = 2 } k2 = { x = 5 y = 6 }",
		tbl()
			 << prop("k1", tbl()
				 << prop("x", 1)
				 << prop("y", 2)
			)
			 << prop("k2", tbl()
				 << prop("x", 5)
				 << prop("y", 6)
			)
	},
};

const SampleList nested_config_samples = {
	{
		R"(k1 = v1
		k2 = false
		k3 = { x = 1 y = 2 s = str }
		k4 = 1.5
		k5 = {
			x = val
			y = { z = 3 table = { param = v } }
		}
		)",
		tbl()
			 << prop("k1", "v1")
			 << prop("k2", false)
			 << prop("k3", tbl()
				 << prop("x", 1)
				 << prop("y", 2)
				 << prop("s", "str")
			)
			 << prop("k4", 1.5)
			 << prop("k5", tbl()
				 << prop("x", "val")
				 << prop("y", tbl()
					 << prop("z", 3)
					 << prop("table", tbl()
						 << prop("param", "v")
					)
				)
			)
	},
};

void test(SampleList::const_reference sample)
{
	auto f = std::make_shared<config::TextView>(sample.first);
	BOOST_TEST(config::parse(f) == *sample.second,
		boost::test_tools::per_element());
}

void error(BadSampleList::const_reference sample)
{
	auto f = std::make_shared<config::TextView>(sample.text);
	BOOST_CHECK_EXCEPTION(config::parse(f), config::SyntaxError,
		[&sample](const config::SyntaxError &exc)
		{
			BOOST_TEST_INFO("exception position " << exc.where());
			BOOST_TEST_INFO("exception: " << exc.what());
			auto msg = std::string{ exc.what() };
			return exc.where() == sample.error_pos
				&& msg.find(sample.error_msg) != std::string::npos;
		});
}
}

BOOST_AUTO_TEST_SUITE(config_parser_tests)

BOOST_DATA_TEST_CASE(test_basic, make(basic_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_basic_bad, make(bad_basic_samples))
{
	error(sample);
}

BOOST_DATA_TEST_CASE(test_key, make(key_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_bool, make(bool_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_int, make(int_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_real, make(real_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_string, make(string_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_table, make(table_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_plain_config, make(plain_config_samples))
{
	test(sample);
}

BOOST_DATA_TEST_CASE(test_nested_config, make(nested_config_samples))
{
	test(sample);
}

BOOST_AUTO_TEST_SUITE_END()