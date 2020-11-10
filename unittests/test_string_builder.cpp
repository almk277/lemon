#include "string_builder.hpp"
#include "arena_imp.hpp"
#include "stub_logger.hpp"
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace
{
constexpr auto n_max = std::numeric_limits<std::size_t>::max();

struct TestCase
{
	std::size_t n;
	std::string s;
};

auto operator<<(std::ostream& stream, const TestCase& test) -> std::ostream&
{
	return stream << "(" << test.n << ", " << test.s << ")";
}

const std::vector<TestCase> integer_samples =
{
	{ 0, "0" },
	{ 1, "1" },
	{ n_max, std::to_string(n_max) },
};
}

BOOST_AUTO_TEST_SUITE(string_builder_tests)

BOOST_DATA_TEST_CASE(test_integer, boost::unit_test::data::make(integer_samples))
{
	ArenaImp a{ slg };
	StringBuilder builder{ a };
	
	BOOST_TEST(builder.convert(sample.n) == sample.s);
}

BOOST_AUTO_TEST_SUITE_END()