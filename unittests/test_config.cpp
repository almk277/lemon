#include "test_config.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/erase_key.hpp>
#include <boost/range/adaptor/indirected.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/scope_exit.hpp>

using namespace config;

using namespace std::string_literals;
//FIXME string not accounted?
using value_types = boost::mpl::set<bool, int_, real, string, table>;
template <typename T>
using value_types_without = typename boost::mpl::erase_key<value_types, T>::type;
using boost::adaptors::indirect;

namespace
{
auto operator""_i(unsigned long long v)
{
	return static_cast<int_>(v);
}
}

auto operator<<(std::ostream &stream, const table::value &v) -> std::ostream&
{
	auto flags = stream.flags();
	BOOST_SCOPE_EXIT(&stream, flags) {
		stream.flags(flags);
	} BOOST_SCOPE_EXIT_END
	stream << std::boolalpha;

	stream << "value{" << v.key() << " -> ";
	if (!v)
		stream << "empty";
	else if (v.is<bool>())
		stream << "bool: " << v.as<bool>();
	else if (v.is<int_>())
		stream << "int: " << v.as<int>();
	else if (v.is<real>())
		stream << "real: " << v.as<real>();
	else if (v.is<string>())
		stream << "string: \"" << v.as<string>() << "\"";
	else if (v.is<table>())
		stream << "table: " << v.as<table>();
	else
		throw std::logic_error
			{ "operator<<(std::ostream &stream, const table::value &v): unknown type" };
	stream << "}";

	return stream;
}

auto operator<<(std::ostream &stream, const table &t) -> std::ostream&
{
	BOOST_CHECK_EXCEPTION(throw 1, int, [](auto &e) {return true; });
	stream << "table {";
	boost::copy(t, std::ostream_iterator<value>{stream, ", "});
	stream << "}";
	return stream;
}

#define CHECK(init) \
do { \
	value v{ "testkey"s, init }; \
	BOOST_TEST(static_cast<bool>(v)); \
	BOOST_TEST(!!v); \
	BOOST_TEST(v.is<decltype(init)>()); \
	BOOST_TEST(v.as<decltype(init)>() == init); \
} while(0)

#define CHECK_THROW(key, expr) \
	BOOST_CHECK_EXCEPTION(expr, table::error, [](auto &exc){ return exc.get_key() == key; })

#define CHECK_BAD_TYPE(init) \
do { \
	value val{ "testkey"s, init }; \
	BOOST_TEST(!val.template is<T>()); \
	CHECK_THROW("testkey", val.template as<T>()); \
} while (false)

BOOST_AUTO_TEST_SUITE(config_tests)

BOOST_AUTO_TEST_CASE(test_empty_value)
{
	const value v{ ""s };
	BOOST_TEST(!static_cast<bool>(v));
	BOOST_TEST(!v);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_empty_value_fail, T, value_types)
{
	CHECK_BAD_TYPE(table::value::empty{});
}

BOOST_AUTO_TEST_CASE(test_bool_value)
{
	CHECK(true);
	CHECK(false);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_bool_value_fail, T, value_types_without<bool>)
{
	CHECK_BAD_TYPE(true);
	CHECK_BAD_TYPE(false);
}

BOOST_AUTO_TEST_CASE(test_int_value)
{
	CHECK(0_i);
	CHECK(1_i);
	CHECK(-1_i);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_int_value_fail, T, value_types_without<int_>)
{
	CHECK_BAD_TYPE(0_i);
	CHECK_BAD_TYPE(1_i);
	CHECK_BAD_TYPE(-1_i);
}

BOOST_AUTO_TEST_CASE(test_real_value)
{
	CHECK(0.0);
	CHECK(1.0);
	CHECK(-1.0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_real_value_fail, T, value_types_without<real>)
{
	CHECK_BAD_TYPE(0.0);
	CHECK_BAD_TYPE(1.0);
	CHECK_BAD_TYPE(-1.0);
}

BOOST_AUTO_TEST_CASE(test_string_value)
{
	CHECK(""s);
	CHECK("str"s);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_string_value_fail, T, value_types_without<string>)
{
	CHECK_BAD_TYPE(""s);
	CHECK_BAD_TYPE("str"s);
}

BOOST_AUTO_TEST_CASE(test_table_value)
{
	CHECK(table{});

	table t1;
	t1.add({ "key1", true })
		.add({ "key2", 42 })
		.add({ "key3", 3.14 })
		.add({ "key4", "val4" })
		;
	CHECK(t1);

	table t2;
	t2.add({ "k1", "v2" })
		.add({ "k2", t1 });
	CHECK(t2);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_table_value_fail, T, value_types_without<table>)
{
	CHECK_BAD_TYPE(table{});
	table t;
	t.add({ "key1", true });
	CHECK_BAD_TYPE(t);
}

BOOST_AUTO_TEST_CASE(test_empty_table)
{
	table t;
	BOOST_TEST(!t["key"].has_value());
	BOOST_TEST(!t.get_unique("key").has_value());
	BOOST_TEST(!t.get_last("key").has_value());
	BOOST_TEST(t.get_all("key").empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_table_bad_key, T, value_types)
{
	table t{};
	CHECK_THROW("badkey", t["badkey"].as<T>());
}

BOOST_AUTO_TEST_CASE(test_plain_table)
{
	table t;
	t.add({ "key1", 1 })
		.add({ "key1", 2 })
		.add({ "key2", 3 })
		.add({ "key1", 4 })
		.add({ "key0", 5 })
	;

	const std::vector<value> vals1 = {
		{ "key1", 1 },
		{ "key1", 2 },
		{ "key1", 4 },
	};
	const std::vector<value> vals2 = {
		{ "key2", 3 },
	};
	const value val0 = { "key0", 5 };
	const std::vector<value> vals = {
		vals1[0],
		vals1[1],
		vals2[0],
		vals1[2],
		val0,
	};

	CHECK_THROW("key1", t["key1"]);
	CHECK_THROW("key1", t.get_unique("key1"));
	BOOST_TEST(t.get_last("key1").as<int_>() == 4);
	auto get_all1 = t.get_all("key1");
	BOOST_TEST(indirect(get_all1) == vals1, boost::test_tools::per_element());

	BOOST_TEST(t["key2"].as<int_>() == 3);
	BOOST_TEST(t.get_unique("key2").as<int_>() == 3);
	BOOST_TEST(t.get_last("key2").as<int_>() == 3);
	auto get_all2 = t.get_all("key2");
	BOOST_TEST(indirect(get_all2) == vals2, boost::test_tools::per_element());

	BOOST_TEST(t == vals, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(test_nested_table)
{
	auto t3 = table{}
		.add({"k31", "v3"});
	auto t2 = table{}
		.add({"k21", "v2"})
		.add({"k22", t3
    });
	auto t1 = table{}
		.add({ "k11", "v1" })
		.add({ "k12", t2
	});

	BOOST_TEST(t1["k11"].as<string>() == "v1");
	BOOST_TEST(t1["k12"].as<table>() == t2);
	BOOST_TEST(t1["k12"].as<table>()["k21"].as<string>() == "v2");
	BOOST_TEST(t1["k12"].as<table>()["k22"].as<table>() == t3);
	BOOST_TEST(t1["k12"].as<table>()["k22"].as<table>()["k31"].as<string>() == "v3");
}

BOOST_AUTO_TEST_CASE(test_nested_table_same_key)
{
	auto t = table{}
		.add({ "k", table{}
			.add({"k", table{}
				.add({"k", 1})}) });

	BOOST_TEST(t["k"].as<table>()["k"].as<table>()["k"].as<int>() == 1);
}

BOOST_AUTO_TEST_CASE(test_plain_table_unknown_key)
{
	table t;
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add({ "k1", 1 });
	CHECK_THROW("k1", t.throw_on_unknown_key());
	static_cast<void>(t["k1"]);
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add({ "k2", 2 })
		.add({ "k2", 20 });
	CHECK_THROW("k2", t.throw_on_unknown_key());
	static_cast<void>(t.get_last("k2"));
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add({ "k3", 3 })
		.add({ "k3", 30 });
	CHECK_THROW("k3", t.throw_on_unknown_key());
	static_cast<void>(t.get_all("k3"));
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());
}

BOOST_AUTO_TEST_CASE(test_nested_table_unknown_key)
{
	table t, t1;
	t1.add({ "k1" , 1 });
	t.add({ "k0", t1 });

	CHECK_THROW("k0", t.throw_on_unknown_key());
	static_cast<void>(t["k0"]);
	CHECK_THROW("k1", t.throw_on_unknown_key());
	static_cast<void>(t["k0"].as<table>()["k1"]);
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());
}

BOOST_AUTO_TEST_SUITE_END()