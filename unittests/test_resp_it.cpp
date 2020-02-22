#include "message.hpp"
#include "stub_logger.hpp"
#include "arena_imp.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <ostream>
#include <numeric>
#include <utility>
#include <string>
#include <vector>

namespace
{
struct test_case
{
	int no;
	std::string response;
	std::vector<std::pair<std::string, std::string>> headers;
	std::vector<std::string> body;
};

struct response_fixture
{
	arena_imp a{ slg };
	response r{ a };

	void fill(const test_case &c)
	{
		r.http_version = message::http_version_type::HTTP_1_1;
		r.code = response::status::OK;
		for (auto &h : c.headers)
			r.headers.emplace_back(h.first, h.second);
		for (auto &b : c.body)
			r.body.emplace_back(b);
	}
};

const auto concat = [](const auto &begin, const auto &end)
{
	return accumulate(begin, end, std::string{},
		[](auto acc, auto s) { return acc + std::string{ s }; });
};

std::ostream &operator<<(std::ostream &s, const test_case &t)
{
	return s << "No " << t.no << ": " << t.response;
}

const std::vector<test_case> response_samples =
{
	{
		1,
		"HTTP/1.1 200 OK\r\n\r\n",
		{},
		{}
	},
	{
		2,
		"HTTP/1.1 200 OK\r\n"
		"\r\n"
		"Body",
		{},
		{ "Body" }
	},
	{
		3,
		"HTTP/1.1 200 OK\r\n"
		"\r\n"
		"Big Body",
		{},
		{ "Big ", "Body" }
	},
	{
		4,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"\r\n",
		{
			{ "User-Agent", "Test" },
		},
		{}
	},
	{
		5,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"\r\n"
		"Body",
{
			{ "User-Agent", "Test" },
		},
		{ "Body" }
	},
	{
		6,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"\r\n"
		"Big Body",
		{
			{ "User-Agent", "Test" },
		},
		{ "Big ", "Body" }
	},
	{
		7,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"h: v\r\n"
		"\r\n",
		{
			{ "User-Agent", "Test" },
			{ "h", "v" },
		},
		{}
	},
	{
		8,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"h: v\r\n"
		"\r\n"
		"Body",
{
			{ "User-Agent", "Test" },
			{ "h", "v" },
		},
		{ "Body" }
	},
	{
		8,
		"HTTP/1.1 200 OK\r\n"
		"User-Agent: Test\r\n"
		"h: v\r\n"
		"\r\n"
		"Big Body",
{
			{ "User-Agent", "Test" },
			{ "h", "v" },
		},
		{ "Big ", "Body" }
	},
};
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(response::const_iterator);

BOOST_FIXTURE_TEST_SUITE(response_iterator_tests, response_fixture)

BOOST_AUTO_TEST_CASE(test_common)
{
	BOOST_TEST(r.begin() == r.begin());
	BOOST_TEST(r.end() == r.end());
	BOOST_TEST(response::const_iterator{} == response::const_iterator{});
	
	response::const_iterator begin{};
	begin = r.begin();
	BOOST_TEST(begin == r.begin());

	BOOST_CHECK_THROW(--r.begin(), std::exception);
	BOOST_CHECK_THROW(++r.end(), std::exception);
	BOOST_CHECK_THROW(*r.end(), std::exception);
}

BOOST_DATA_TEST_CASE(test_increment, boost::unit_test::data::make(response_samples))
{
	fill(sample);

	auto response_string = concat(r.begin(), r.end());
	BOOST_TEST(response_string == sample.response);
}

BOOST_DATA_TEST_CASE(test_decrement, boost::unit_test::data::make(response_samples))
{
	fill(sample);

	std::vector<string_view> reverse_chunks{
		std::make_reverse_iterator(r.end()),
		std::make_reverse_iterator(r.begin()) };
	auto reverse_response_string = concat(
		std::make_reverse_iterator(reverse_chunks.end()),
		std::make_reverse_iterator(reverse_chunks.begin()));
	BOOST_TEST(reverse_response_string == sample.response);
}

BOOST_AUTO_TEST_SUITE_END()