#include "test_config.hpp"
#include "visitor.hpp"
#include <boost/mp11/tuple.hpp>
#include <boost/range/adaptor/indirected.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/scope_exit.hpp>
#include <tuple>
#include <type_traits>

using namespace config;
using namespace test;
using namespace std::string_literals;
using boost::unit_test::data::make;

//FIXME string not accounted?
using ValueTypes = std::tuple<Boolean, Integer, Real, String, Table>;
using boost::adaptors::indirect;

namespace
{
auto operator""_i(unsigned long long v)
{
	return static_cast<Integer>(v);
}
}

auto config::operator<<(std::ostream& stream, const Property& p) -> std::ostream&
{
	auto flags = stream.flags();
	BOOST_SCOPE_EXIT(&stream, flags) {
		stream.flags(flags);
	} BOOST_SCOPE_EXIT_END
	stream << std::boolalpha;

	stream << "property{\"" << p.key() << "\" -> ";

	if (p)
		Visit<Boolean, Integer, Real, String, Table>{}(p, Visitor{
			[&stream](Boolean v) { stream << "boolean: " << v; },
			[&stream](Integer v) { stream << "integer: " << v; },
			[&stream](Real v) { stream << "real: " << v; },
			[&stream](const String& v) { stream << "string: \"" << v << "\""; },
			[&stream](const Table& v) { stream << "table: " << v; },
		});
	else
		stream << "empty";

	return stream << "}";
}

auto config::operator<<(std::ostream& stream, const Property::EmptyType&) -> std::ostream&
{
	return stream << "empty";
}

auto config::operator<<(std::ostream& stream, const Table& t) -> std::ostream&
{
	stream << "table {";
	boost::copy(t, std::ostream_iterator<Property>{stream, ", "});
	stream << "}";
	return stream;
}

#define CHECK_THROW(_key, expr) \
	BOOST_CHECK_EXCEPTION(expr, config::BadKey, [](auto &exc){ return exc.key() == (_key); })

template <typename InitType>
void check_good_type(const Property& p, const InitType& init)
{
	BOOST_TEST(static_cast<bool>(p));
	BOOST_TEST(!!p);
	BOOST_TEST(p.is<InitType>());
	BOOST_TEST(p.as<InitType>() == init);
	if constexpr (Property::scalar_type<InitType>)
		BOOST_TEST(p.as_opt<InitType>().value() == init);
}

template <typename TestType>
void check_bad_type(const Property& p)
{
	BOOST_TEST_CONTEXT("TestType=" << typeid(TestType).name()) {
		BOOST_TEST(!p.is<TestType>());
		CHECK_THROW("testkey", p.as<TestType>());
	}
}

template <typename InitType, typename TestType>
void check_type(const Property& p, const InitType& init)
{
	using PlainInitType = std::remove_cv_t<std::remove_reference_t<InitType>>;
	if constexpr(std::is_same_v<TestType, PlainInitType>)
		check_good_type<PlainInitType>(p, init);
	else
		check_bad_type<TestType>(p);
}

template <typename InitType>
void check(InitType&& init1, InitType&& init2)
{
	Property p{ std::make_unique<PropertyErrorHandler>(), "testkey"s, std::forward<InitType>(init1) };
	BOOST_TEST_CONTEXT(p) {
		check_type<InitType, Boolean>(p, init2);
		check_type<InitType, Integer>(p, init2);
		check_type<InitType, Real>(p, init2);
		check_type<InitType, String>(p, init2);
		check_type<InitType, Table>(p, init2);
	}
}

template <typename InitType>
std::enable_if_t<std::is_copy_constructible_v<InitType>>
check(const InitType& init)
{
	check(init, init);
}

#define CHECK(init) check((init), (init))

BOOST_AUTO_TEST_SUITE(config_tests)

BOOST_AUTO_TEST_CASE(test_empty_value)
{
	const Property v{ std::make_unique<PropertyErrorHandler>(), "testkey"s };
	BOOST_TEST(!static_cast<bool>(v));
	BOOST_TEST(!v);
	
	using Scalar = std::tuple<Boolean, Integer, Real, String>;
	boost::mp11::tuple_for_each(Scalar(), [&v](auto x)
	{
			BOOST_TEST(!v.as_opt<decltype(x)>());
	});
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_empty_value_fail, T, ValueTypes)
{
	check_bad_type<T>(Property{ std::make_unique<PropertyErrorHandler>(),  "testkey" });
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

BOOST_AUTO_TEST_CASE_TEMPLATE(test_table_bad_key, T, ValueTypes)
{
	auto t = tbl();
	CHECK_THROW("badkey", t["badkey"].as<T>());
}

struct List
{
	std::vector<Property> Data;

	List&& operator<<(Property&& p) &&
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

	std::vector<Property> vals2;
	vals2.push_back(prop("key2", 3));

	const auto vals = List{}
		<< prop("key1", 1)
		<< prop("key1", 2)
		<< prop("key2", 3)
		<< prop("key1", 4)
		<< prop("key0", 5)
		;

	CHECK_THROW("key1", static_cast<void>(t["key1"]));
	CHECK_THROW("key1", static_cast<void>(t.get_unique("key1")));
	BOOST_TEST(t.get_last("key1").as<Integer>() == 4);
	auto get_all1 = t.get_all("key1");
	BOOST_TEST(indirect(get_all1) == vals1.Data, boost::test_tools::per_element());

	BOOST_TEST(t["key2"].as<Integer>() == 3);
	BOOST_TEST(t.get_unique("key2").as<Integer>() == 3);
	BOOST_TEST(t.get_last("key2").as<Integer>() == 3);
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

	BOOST_TEST(t1["k11"].as<String>() == "v1");
	BOOST_TEST(t1["k12"].as<Table>() == t2_gen());
	BOOST_TEST(t1["k12"].as<Table>()["k21"].as<String>() == "v2");
	BOOST_TEST(t1["k12"].as<Table>()["k22"].as<Table>() == t3_gen());
	BOOST_TEST(t1["k12"].as<Table>()["k22"].as<Table>()["k31"].as<String>() == "v3");
}

BOOST_AUTO_TEST_CASE(test_nested_table_same_key)
{
	auto t = tbl()
		<< prop("k", tbl()
			<< prop("k", tbl()
				<< prop("k", 1)));

	BOOST_TEST(t["k"].as<Table>()["k"].as<Table>()["k"].as<Integer>() == 1);
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
	static_cast<void>(t["k0"].as<Table>()["k1"]);
	BOOST_CHECK_NO_THROW(t.throw_on_unknown_key());
}

BOOST_AUTO_TEST_CASE(test_visit_int)
{
	enum Type
	{
		boolean,
		integer,
		real,
		string,
		table,
	};

	auto p = prop("k", 12);
	struct Vis
	{
		auto operator()(bool) { return boolean; }
		auto operator()(int) { return integer; }
		auto operator()(double) { return real; }
		auto operator()(string_view) { return string; }
		auto operator()(const Table&) { return table; }
	} v;

	BOOST_TEST((Visit<Boolean, Integer, Real, String, Table>()(p, v)) == integer);
	BOOST_TEST((Visit<Integer, Real, Table>()(p, v)) == integer);
	BOOST_TEST((Visit<Integer>()(p, v)) == integer);

	auto make_predicate = [](string_view expected)
	{
		return [expected](const BadValue& exc)
		{
			return exc.key() == "k" && exc.obtained() == "integer" && exc.expected() == expected;
		};
	};
	BOOST_CHECK_EXCEPTION((Visit<Boolean, Real, String>()(p, v)), BadValue, make_predicate("[boolean, real, string]"));
	BOOST_CHECK_EXCEPTION((Visit<Real, Table>()(p, v)), BadValue, make_predicate("[real, table]"));
}

BOOST_AUTO_TEST_SUITE_END()