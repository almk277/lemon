#include "test_config.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/mpl/set.hpp>
#include <boost/range/adaptor/indirected.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/scope_exit.hpp>
#include <type_traits>

using namespace config;
using namespace test;
using namespace std::string_literals;
using boost::unit_test::data::make;

//FIXME string not accounted?
using value_types = boost::mpl::set<boolean, integer, real, string, table>;
using boost::adaptors::indirect;

namespace
{
auto operator""_i(unsigned long long v)
{
	return static_cast<integer>(v);
}

auto tbl()
{
	return table(std::make_unique<table_error_handler>());
}
}

auto config::operator<<(std::ostream& stream, const property &p) -> std::ostream&
{
	auto flags = stream.flags();
	BOOST_SCOPE_EXIT(&stream, flags) {
		stream.flags(flags);
	} BOOST_SCOPE_EXIT_END
	stream << std::boolalpha;

	stream << "property{\"" << p.key() << "\" -> ";
	if (!p)
		stream << "empty";
	else if (p.is<boolean>())
		stream << "boolean: " << p.as<boolean>();
	else if (p.is<integer>())
		stream << "integer: " << p.as<integer>();
	else if (p.is<real>())
		stream << "real: " << p.as<real>();
	else if (p.is<string>())
		stream << "string: \"" << p.as<string>() << "\"";
	else if (p.is<table>())
		stream << "table: " << p.as<table>();
	else
		throw std::logic_error
			{ "operator<<(std::ostream &stream, const property &v): unknown type" };
	stream << "}";

	return stream;
}

auto config::operator<<(std::ostream& stream, const property::empty_type&) -> std::ostream&
{
	return stream << "empty";
}

auto config::operator<<(std::ostream &stream, const table &t) -> std::ostream&
{
	stream << "table {";
	boost::copy(t, std::ostream_iterator<property>{stream, ", "});
	stream << "}";
	return stream;
}

#define CHECK_THROW(_key, expr) \
	BOOST_CHECK_EXCEPTION(expr, config::bad_key, [](auto &exc){ return exc.key() == (_key); })

template <typename InitType>
void check_good_type(const property &p, const InitType &init)
{
	BOOST_TEST(static_cast<bool>(p));
	BOOST_TEST(!!p);
	BOOST_TEST(p.is<InitType>());
	BOOST_TEST(p.as<InitType>() == init);
}

template <typename TestType>
void check_bad_type(const property &p)
{
	BOOST_TEST_CONTEXT("TestType=" << typeid(TestType).name()) {
		BOOST_TEST(!p.is<TestType>());
		CHECK_THROW("testkey", p.as<TestType>());
	}
}

template <typename InitType, typename TestType>
void check_type(const property &p, const InitType &init)
{
	using PlainInitType = std::remove_cv_t<std::remove_reference_t<InitType>>;
	auto same = std::is_same<TestType, PlainInitType>::value;
	if (same)
		check_good_type<PlainInitType>(p, init);
	else
		check_bad_type<TestType>(p);
}

template <typename InitType>
void check(InitType &&init1, InitType &&init2)
{
	property p{ std::make_unique<property_error_handler>(), "testkey"s, std::forward<InitType>(init1) };
	BOOST_TEST_CONTEXT(p) {
		check_type<InitType, boolean>(p, init2);
		check_type<InitType, integer>(p, init2);
		check_type<InitType, real>(p, init2);
		check_type<InitType, string>(p, init2);
		check_type<InitType, table>(p, init2);
	}
}

template <typename InitType>
std::enable_if_t<std::is_copy_constructible<InitType>::value>
check(const InitType &init)
{
	check(init, init);
}

#define CHECK(init) check((init), (init))

BOOST_AUTO_TEST_SUITE(config_tests)

BOOST_AUTO_TEST_CASE(test_empty_value)
{
	const property v{ std::make_unique<property_error_handler>(), "testkey"s };
	BOOST_TEST(!static_cast<bool>(v));
	BOOST_TEST(!v);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_empty_value_fail, T, value_types)
{
	check_bad_type<T>(property{ std::make_unique<property_error_handler>(),  "testkey" });
}

BOOST_DATA_TEST_CASE(test_bool_value, make({false, true}))
{
	check(sample);
}

BOOST_DATA_TEST_CASE(test_int_value, make({0_i, 1_i, -1_i}))
{
	check(sample);
}

BOOST_DATA_TEST_CASE(test_real_value, make({0.0, 1.0, -1.0}))
{
	check(sample);
}

BOOST_DATA_TEST_CASE(test_string_value, make({""s, "str"s}))
{
	check(sample);
}

BOOST_AUTO_TEST_CASE(test_table_value)
{
	CHECK(tbl());
	CHECK(tbl()
		<< prop("key1", true)
		<< prop("key2", 42)
		<< prop("key3", 3.14)
		<< prop("key4", "val4"));
	CHECK(tbl()
		<< prop("k1", "v2")
		<< prop("k2", tbl()
			<< prop("key1", true)));
}

BOOST_AUTO_TEST_CASE(test_empty_table)
{
	auto t = tbl();
	BOOST_TEST(!t["key"].has_value());
	BOOST_TEST(!t.get_unique("key").has_value());
	BOOST_TEST(!t.get_last("key").has_value());
	BOOST_TEST(t.get_all("key").empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_table_bad_key, T, value_types)
{
	auto t = tbl();
	CHECK_THROW("badkey", t["badkey"].as<T>());
}

struct List
{
	std::vector<property> Data;

	List &&operator<<(property &&p) &&
	{
		Data.push_back(std::move(p));
		return std::move(*this);
	}
};

BOOST_AUTO_TEST_CASE(test_plain_table)
{
	auto t = tbl()
		<< prop("key1", 1)
		<< prop("key1", 2)
		<< prop("key2", 3)
		<< prop("key1", 4)
		<< prop("key0", 5)
	;

	auto vals1 = List{}
		<< prop("key1", 1)
		<< prop("key1", 2)
		<< prop("key1", 4)
		;

	std::vector<property> vals2;
	vals2.push_back(prop("key2", 3));

	const auto vals = List{}
		<< prop("key1", 1)
		<< prop("key1", 2)
		<< prop("key2", 3)
		<< prop("key1", 4)
		<< prop("key0", 5)
		;

	CHECK_THROW("key1", t["key1"]);
	CHECK_THROW("key1", t.get_unique("key1"));
	BOOST_TEST(t.get_last("key1").as<integer>() == 4);
	auto get_all1 = t.get_all("key1");
	BOOST_TEST(indirect(get_all1) == vals1.Data, boost::test_tools::per_element());

	BOOST_TEST(t["key2"].as<integer>() == 3);
	BOOST_TEST(t.get_unique("key2").as<integer>() == 3);
	BOOST_TEST(t.get_last("key2").as<integer>() == 3);
	auto get_all2 = t.get_all("key2");
	BOOST_TEST(indirect(get_all2) == vals2, boost::test_tools::per_element());

	BOOST_TEST(t == vals.Data, boost::test_tools::per_element());
}

BOOST_AUTO_TEST_CASE(test_nested_table)
{
	auto t3_gen = [] {
		return tbl()
			<< prop("k31", "v3");
	};
	auto t2_gen = [t3_gen] {
		return tbl()
			<< prop("k21", "v2")
			<< prop("k22", t3_gen());
	};
	auto t1 = tbl()
		<< prop("k11", "v1" )
		<< prop("k12", t2_gen());

	BOOST_TEST(t1["k11"].as<string>() == "v1");
	BOOST_TEST(t1["k12"].as<table>() == t2_gen());
	BOOST_TEST(t1["k12"].as<table>()["k21"].as<string>() == "v2");
	BOOST_TEST(t1["k12"].as<table>()["k22"].as<table>() == t3_gen());
	BOOST_TEST(t1["k12"].as<table>()["k22"].as<table>()["k31"].as<string>() == "v3");
}

BOOST_AUTO_TEST_CASE(test_nested_table_same_key)
{
	auto t = tbl()
		<< prop("k", tbl()
			<< prop("k", tbl()
				<< prop("k", 1)));

	BOOST_TEST(t["k"].as<table>()["k"].as<table>()["k"].as<integer>() == 1);
}

BOOST_AUTO_TEST_CASE(test_plain_table_unknown_key)
{
	auto t = tbl();
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add(prop("k1", 1));
	CHECK_THROW("k1", t.throw_on_unknown_key());
	static_cast<void>(t["k1"]);
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add(prop("k2", 2))
		.add(prop("k2", 20));
	CHECK_THROW("k2", t.throw_on_unknown_key());
	static_cast<void>(t.get_last("k2"));
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());

	t.add(prop("k3", 3))
		.add(prop("k3", 30));
	CHECK_THROW("k3", t.throw_on_unknown_key());
	static_cast<void>(t.get_all("k3"));
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());
}

BOOST_AUTO_TEST_CASE(test_nested_table_unknown_key)
{
	auto t = tbl()
		<< prop("k0", tbl()
			<< prop("k1", 1));

	CHECK_THROW("k0", t.throw_on_unknown_key());
	static_cast<void>(t["k0"]);
	CHECK_THROW("k1", t.throw_on_unknown_key());
	static_cast<void>(t["k0"].as<table>()["k1"]);
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());
}

BOOST_AUTO_TEST_SUITE_END()
