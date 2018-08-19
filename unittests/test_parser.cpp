#include "parser.hpp"
#include "arena_imp.hpp"
#include "stub_logger.hpp"
#include "message.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/mpl/bool.hpp>
#include <boost/variant/get.hpp>
#include <deque>
#include <utility>

namespace {
	std::string body(const request &r)
	{
		std::string b;
		for (auto s : r.body)
			b += std::string{ s };
		return b;
	}

	struct parser_fixture
	{
		arena_imp a{ slg };
		request req{a};
		parser p;

		void reset()
		{
			req.url = {};
			req.headers.clear();
			req.body.clear();
			p.reset(req, a);
		}
		parser_fixture()
		{
			reset();
		}
	};

	struct good_test_case
	{
		int no;
		std::string request;
		request::method_s::type_e method_type;
		string_view method_name;
		string_view url;
		message::http_version_type version;
		std::vector<request::header> headers;
		std::string body;
	};

	std::ostream &operator<<(std::ostream &s, const good_test_case &t)
	{
		return s << "No " << t.no << ": " << t.request;
	}

	struct bad_test_case
	{
		int no;
		std::string request;
		boost::optional<response_status> code;
	};

	std::ostream &operator<<(std::ostream &s, const bad_test_case &t)
	{
		return s << "No " << t.no << ": " << t.request;
	}
}

static std::ostream &operator<<(std::ostream &s, request::http_version_type v)
{
	return s << static_cast<int>(v);
}

static std::ostream &operator<<(std::ostream &s, request::method_s::type_e t)
{
	return s << static_cast<int>(t);
}

static std::ostream &operator<<(std::ostream &s, const request::header &h)
{
	return s << h.name << ": " << h.value;
}

static std::ostream &operator<<(std::ostream &s, const string_view &sw)
{
	return s << std::string{ sw };
}

const std::vector<good_test_case> good_request_samples = {
	{
		1,
		"POST / HTTP/1.1\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"Hello",
		request::method_s::type_e::POST,
		"POST",
		"/",
		message::http_version_type::HTTP_1_1,
		{
			{ "Content-Type", "text/plain" },
			{ "Content-Length", "5" },
		},
		"Hello"
	},
	{
		2,
		"POST / HTTP/1.1\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 4\r\n"
		"User-Agent: Lemon\r\n"
		"\r\n"
		"body",
		request::method_s::type_e::POST,
		"POST",
		"/",
		message::http_version_type::HTTP_1_1,
		{
			{ "Content-Type", "text/plain" },
			{ "Content-Length", "4" },
			{ "User-Agent", "Lemon" },
		},
		"body"
	},
	{
		3,
		"PUT / HTTP/1.1\r\n\r\n",
		request::method_s::type_e::OTHER,
		"PUT",
		"/",
		message::http_version_type::HTTP_1_1,
		{},
		{}
	},
};

const std::vector<good_test_case> good_last_request_samples = {
	{
		101,
		"GET / HTTP/1.0\r\n\r\n",
		request::method_s::type_e::GET,
		"GET",
		"/",
		message::http_version_type::HTTP_1_0,
		{},
		{}
	},
	{
		102,
		"POST /index HTTP/1.1\r\n"
		"Connection: close\r\n"
		"\r\n",
		request::method_s::type_e::POST,
		"POST",
		"/index",
		message::http_version_type::HTTP_1_1,
		{
			{"Connection", "close"},
		},
		{}
	},
};

const std::vector<bad_test_case> bad_request_samples = {
	{
		201,
		"POST / HTTP/1.1\r\n"
		"BadHeader\r\n",
		{}
	},
	{
		202,
		"POST / HTTP/2.0\r\n\r\n",
		response_status::HTTP_VERSION_NOT_SUPPORTED
	},
	{
		203,
		"POST @address HTTP/1.1\r\n\r\n",
		{}
	},
};

struct fragmented_pipeline_dataset
{
	struct sample: boost::noncopyable
	{
		sample(std::deque<good_test_case> cases, std::size_t pos):
			cases{cases}
		{
			for (auto &t : cases)
				request += t.request;
			chunks = {
				{ request.data(), pos },
				{ request.data() + pos, request.size() - pos }
			};
			
			check();
		}
		sample(sample &&rhs) noexcept:
			request{move(rhs.request)},
			chunks{std::move(rhs.chunks)},
			cases{move(rhs.cases)}
		{
			check();
		}

		void check() const
		{
			string_view prev{};
			for (auto c: chunks) {
				BOOST_ASSERT(prev.empty() || prev.end() == c.begin());
				prev = c;
			}
		}

		std::string request;
		std::deque<string_view> chunks;
		std::deque<good_test_case> cases;
	};

	struct iterator
	{
		explicit iterator(std::deque<good_test_case> cases) :
			cases{ move(cases) } {}

		sample operator*() const
		{
			return { cases, pos };
		}

		iterator &operator++()
		{
			++pos;
			return *this;
		}

	private:
		std::deque<good_test_case> cases;
		std::size_t pos = 1;
	};

	enum { arity = 1 };

	boost::unit_test::data::size_t size() const
	{
		std::size_t size = 0;
		for (auto &t : cases)
			size += t.request.size();
		return size - 1;
	}

	iterator begin() const { return iterator{ cases }; }

private:
	std::deque<good_test_case> cases = {
		good_request_samples.at(0),
		good_request_samples.at(1)
	};
};

namespace boost {
namespace unit_test {
namespace data {
namespace monomorphic {
template <>
struct is_dataset<fragmented_pipeline_dataset> : mpl::true_ {};
}}}}

static std::ostream &operator<<(std::ostream &s,
	const fragmented_pipeline_dataset::sample &ds)
{
	s << "[";
	for (auto &c : ds.chunks)
		s << "\"" << c << "\" ";
	s << "]";
	return s;
}

BOOST_FIXTURE_TEST_SUITE(parser_tests, parser_fixture)

BOOST_DATA_TEST_CASE(test_simple,
	boost::unit_test::data::make(good_request_samples)
	+ boost::unit_test::data::make(good_last_request_samples))
{
	auto result = p.parse_chunk(sample.request);

	auto error = boost::get<http_error>(&result);
	if (error)
		BOOST_TEST_MESSAGE(std::to_string(static_cast<int>(error->code))
			+ " " + std::string{ error->details });
	BOOST_TEST_REQUIRE(!error);

	auto r = boost::get<parser::complete_request>(&result);
	BOOST_TEST_REQUIRE(r);
	BOOST_TEST(r->rest.empty());
	BOOST_TEST(req.method.type == sample.method_type);
	BOOST_TEST(req.method.name == sample.method_name);
	BOOST_TEST(req.url.path == sample.url);
	BOOST_TEST(req.http_version == sample.version);
	BOOST_TEST(req.headers == sample.headers, boost::test_tools::per_element());
	BOOST_TEST(body(req) == sample.body);
}

BOOST_DATA_TEST_CASE(test_pipeline, fragmented_pipeline_dataset{})
{
	auto chunks = sample.chunks;
	auto cases = sample.cases;

	while (!chunks.empty())
	{
		auto chunk = chunks.front();
		chunks.pop_front();
		auto result = p.parse_chunk(chunk);

		auto result_error = boost::get<http_error>(&result);
		BOOST_TEST(!result_error);

		if (auto result_complete = boost::get<parser::complete_request>(&result))
		{
			if (!result_complete->rest.empty())
				chunks.emplace_front(result_complete->rest);

			BOOST_TEST(!cases.empty());
			auto expected = cases.front();
			cases.pop_front();

			BOOST_TEST(req.method.type == expected.method_type);
			BOOST_TEST(req.method.name == expected.method_name);
			BOOST_TEST(req.url.path == expected.url);
			BOOST_TEST(req.http_version == expected.version);
			BOOST_TEST(req.headers == expected.headers, boost::test_tools::per_element());
			BOOST_TEST(body(req) == expected.body);

			reset();
		}
	}

	BOOST_TEST(cases.empty());
}

BOOST_DATA_TEST_CASE(test_errors, boost::unit_test::data::make(bad_request_samples))
{
	auto result = p.parse_chunk(sample.request);

	auto r = boost::get<http_error>(&result);
	BOOST_TEST_REQUIRE(r);
	BOOST_TEST(r->code == sample.code.value_or(response_status::BAD_REQUEST));
}

BOOST_AUTO_TEST_SUITE_END()