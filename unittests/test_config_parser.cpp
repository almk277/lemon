#include "config_parser.hpp"
#include "test_config.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <initializer_list>
#include <utility>

using boost::unit_test::data::make;

struct position
{
	constexpr position(std::size_t v): val{ v } {}

	std::size_t val;
};

using sample_list = std::initializer_list<std::pair<string_view, table>>;
using bad_sample_list = std::initializer_list<std::pair<string_view, position>>;

static std::ostream &operator<<(std::ostream &stream, sample_list::const_reference sample)
{
	return stream << "TEXT{\"" << sample.first << "\"} -> " << sample.second;
}

static std::ostream &operator<<(std::ostream &stream, bad_sample_list::const_reference sample)
{
	return stream << "TEXT{\"" << sample.first << "\"} -> error at " << sample.second.val;
}

namespace
{
const sample_list basic_samples = {
	{
		"", table{}
	},
	{
		" \t\n\t ", table{}
	},
	{
		"  k  =  true  ",
		table{}.add({"k", true})
	},
	{
		"\tk\t=\ttrue\t",
		table{}.add({"k", true})
	},
	{
		"\nk\n=\ntrue\n",
		table{}.add({"k", true})
	},
	{
		" \t \nk\t \n =\t \n true\t \n ",
		table{}.add({"k", true})
	},
};

const bad_sample_list bad_basic_samples = {
	{ "abc", 3 },
	{ "a b", 2 },
	{ "a 0", 2 },
	{ "a =", 3 },
	{ "a = b c", 7 },
	{ "a = b c =", 9 },
	{ "a = {", 5 },
	{ "a = { b", 7 },
	{ "a = { b =", 9 },
	{ "a = { b = c", 11 },
};

const sample_list key_samples = {
	{
		"snake_key_name = 1",
		table{}.add({"snake_key_name", 1})
	},
	{
		"CamelNameKey = 1",
		table{}.add({"CamelNameKey", 1})
	},
	{
		"key.dot.x = 1",
		table{}.add({"key.dot.x", 1})
	},
	{
		"key0123 = 1",
		table{}.add({"key0123", 1})
	},
	{
		"Real_LONG_012.Key9 = 1",
		table{}.add({"Real_LONG_012.Key9", 1})
	},
	{
		"trueKey = 1",
		table{}.add({"trueKey", 1})
	},
};

const sample_list bool_samples = {
	{
		"key = true",
		table{}.add({"key", true})
	},
	{
		"key = false",
		table{}.add({"key", false})
	},
	{
		"key = true ",
		table{}.add({"key", true})
	},
};

const sample_list int_samples = {
	{
		"key = 0",
		table{}.add({"key", 0})
	},
	{
		"key = -2147483647",
		table{}.add({"key", -2147483647})
	},
	{
		"key = +2147483647",
		table{}.add({"key", +2147483647})
	},
	{
		"key = 2147483647",
		table{}.add({"key", 2147483647})
	},
};

const sample_list real_samples = {
	{
		"key = 0.0",
		table{}.add({"key", 0.0})
	},
	{
		"key = 12345.6789",
		table{}.add({"key", 12345.6789})
	},
	{
		"key = -12345.6789",
		table{}.add({"key", -12345.6789})
	},
	{
		"key = -1.12345e-38",
		table{}.add({"key", -1.12345e-38})
	},
	{
		"key = -1.12345e+38",
		table{}.add({"key", -1.12345e+38})
	},
	{
		"key = +1.12345e-38",
		table{}.add({"key", +1.12345e-38})
	},
	{
		"key = +1.12345e+38",
		table{}.add({"key", +1.12345e+38})
	},
	{
		"key = 1.12345e-38",
		table{}.add({"key", 1.12345e-38})
	},
	{
		"key = 1.12345e+38",
		table{}.add({"key", 1.12345e+38})
	},
};

const sample_list string_samples = {
	{
		"key = value",
		table{}.add({"key", "value"})
	},
	{
		"key = truestring",
		table{}.add({"key", "truestring"})
	},
	{
		"key = True",
		table{}.add({"key", "True"})
	},
	{
		"key = a0123456789",
		table{}.add({"key", "a0123456789"})
	},
	{
		R"(key = a/\._-z)",
		table{}.add({"key", R"(a/\._-z)"})
	},
	{
		"key = 'value'",
		table{}.add({"key", "value"})
	},
	{
		"key = ''",
		table{}.add({"key", ""})
	},
	{
		"key = ' x '",
		table{}.add({"key", " x "})
	},
	{
		"key = 'a b\tc'",
		table{}.add({"key", "a b\tc"})
	},
	{
		"key = 'true'",
		table{}.add({"key", "true"})
	},
	{
		"key = '1'",
		table{}.add({"key", "1"})
	},
	{
		"key = '0.1'",
		table{}.add({"key", "0.1"})
	},
	{
		"key = '{ x = 0 }'",
		table{}.add({"key", "{ x = 0 }"})
	},
};

const sample_list table_samples = {
	{
		"k = { }",
		table{}.add({"k", table{}})
	},
	{
		"k = { x = 1 }",
		table{}.add({"k", table{}.add({"x", 1})})
	},
	{
		"k = {\n\tx = 1\n}",
		table{}.add({"k", table{}.add({"x", 1})})
	},
	{
		"k = \n{\n  x = 1\n}",
		table{}.add({"k", table{}.add({"x", 1})})
	},
	{
		"k = { x = 1 y = 2 }",
		table{}.add({"k", table{}
			.add({"x", 1})
			.add({"y", 2})
		})
	},
};

const sample_list plain_config_samples = {
	{
		"key1 = true key2 = 42 key3 = 3.14 key4 = some_string",
		table{}
			.add({ "key1", true })
			.add({ "key2", 42 })
			.add({ "key3", 3.14 })
			.add({ "key4", "some_string" })
	},
	{
		R"(
		key1 = true
		key2 = 42

		key3 = 3.14


		key4 = some_string
	)",
	table{}
		.add({ "key1", true })
		.add({ "key2", 42 })
		.add({ "key3", 3.14 })
		.add({ "key4", "some_string" })
},
{
	"k1 = { x = 1 y = 2 } k2 = { x = 5 y = 6 }",
	table{}
		.add({"k1", table{}
			.add({"x", 1})
			.add({"y", 2})
		})
		.add({"k2", table{}
			.add({"x", 5})
			.add({"y", 6})
		})
},
};

const sample_list nested_config_samples = {
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
	table{}
		.add({"k1", "v1"})
		.add({"k2", false})
		.add({"k3", table{}
			.add({"x", 1})
			.add({"y", 2})
			.add({"s", "str"})
		})
		.add({"k4", 1.5})
		.add({"k5", table{}
			.add({"x", "val"})
			.add({"y", table{}
				.add({"z", 3})
				.add({"table", table{}
					.add({"param", "v"})
				})
			})
		})
}
};

void test(sample_list::const_reference sample)
{
	BOOST_TEST(config::parse(sample.first) == sample.second,
		boost::test_tools::per_element());
}

void error(bad_sample_list::const_reference sample)
{
	BOOST_CHECK_EXCEPTION(config::parse(sample.first), config::error,
		[pos = sample.second.val](const config::error &exc)
		{
			BOOST_TEST_INFO("exception position " << exc.position);
			BOOST_TEST_INFO("exception: " << exc.what());
			return exc.position == pos;
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