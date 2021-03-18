#include "http_parser_.hpp"
#include "arena_imp.hpp"
#include "http_message.hpp"
#include "stub_logger.hpp"
#include <boost/mpl/bool.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <deque>
#include <utility>

using namespace http;

namespace
{
std::string body(const Request& r)
{
	std::string b;
	for (auto s : r.body)
		b += std::string{ s };
	return b;
}

struct ParserFixture
{
	ArenaImp a{ slg };
	Request req{a};
	Parser p;

	void reset()
	{
		req.url = {};
		req.headers.clear();
		req.body.clear();
		p.reset(req);
	}
	ParserFixture()
	{
		reset();
	}
};

struct GoodTestCase
{
	int no;
	std::string request;
	Request::Method::Type method_type;
	string_view method_name;
	string_view url;
	Message::ProtocolVersion version;
	std::vector<Request::Header> headers;
	std::string body;

	auto lowercased_headers() const
	{
		decltype(headers) result{headers.begin(), headers.end()};
		for (auto& hdr : result)
		{
			hdr.lowercase_name = hdr.name;
		}
		return result;
	}
};

std::ostream& operator<<(std::ostream& s, const GoodTestCase& t)
{
	return s << "No " << t.no << ": " << t.request;
}

struct BadTestCase
{
	int no;
	std::string request;
	boost::optional<Response::Status> code;
};

std::ostream& operator<<(std::ostream& s, const BadTestCase& t)
{
	return s << "No " << t.no << ": " << t.request;
}
}

namespace http
{
static std::ostream& operator<<(std::ostream& s, Request::ProtocolVersion v)
{
	return s << static_cast<int>(v);
}

static std::ostream& operator<<(std::ostream& s, Request::Method::Type t)
{
	return s << static_cast<int>(t);
}

static std::ostream& operator<<(std::ostream& s, const Request::Header& h)
{
	return s << h.name << ": " << h.value;
}
}

static std::ostream& operator<<(std::ostream& s, const string_view& sw)
{
	return s << std::string{ sw };
}

const std::vector<GoodTestCase> good_request_samples = {
	{
		1,
		"POST / HTTP/1.1\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 5\r\n"
		"\r\n"
		"Hello",
		Request::Method::Type::post,
		"POST",
		"/",
		Message::ProtocolVersion::http_1_1,
		{
			{ "content-type", "text/plain" },
			{ "content-length", "5" },
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
		Request::Method::Type::post,
		"POST",
		"/",
		Message::ProtocolVersion::http_1_1,
		{
			{ "content-type", "text/plain" },
			{ "content-length", "4" },
			{ "user-agent", "Lemon" },
		},
		"body"
	},
	{
		3,
		"PUT / HTTP/1.1\r\n\r\n",
		Request::Method::Type::other,
		"PUT",
		"/",
		Message::ProtocolVersion::http_1_1,
		{},
		{}
	},
	{
		4,
		"POST / HTTP/1.1\r\n"
		"Transfer-Encoding: chunked\r\n"
		"\r\n"
		"4\r\n"
		"chun\r\n"
		"6\r\n"
		"ked da\r\n"
		"2\r\n"
		"ta\r\n"
		"0\r\n"
		"\r\n",
		Request::Method::Type::post,
		"POST",
		"/",
		Message::ProtocolVersion::http_1_1,
		{
			{ "transfer-encoding", "chunked" },
		},
		"chunked data"
	},
};

const std::vector<GoodTestCase> good_last_request_samples = {
	{
		101,
		"GET / HTTP/1.0\r\n\r\n",
		Request::Method::Type::get,
		"GET",
		"/",
		Message::ProtocolVersion::http_1_0,
		{},
		{}
	},
	{
		102,
		"POST /index HTTP/1.1\r\n"
		"Connection: close\r\n"
		"\r\n",
		Request::Method::Type::post,
		"POST",
		"/index",
		Message::ProtocolVersion::http_1_1,
		{
			{"connection", "close"},
		},
		{}
	},
};

const std::vector<BadTestCase> bad_request_samples = {
	{
		201,
		"POST / HTTP/1.1\r\n"
		"BadHeader\r\n",
		{}
	},
	{
		202,
		"POST / HTTP/2.0\r\n\r\n",
		Response::Status::http_version_not_supported
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
		sample(std::deque<GoodTestCase> cases, std::size_t pos):
			cases{cases}
		{
			for (auto& t : cases)
				request += t.request;
			chunks = {
				{ request.data(), pos },
				{ request.data() + pos, request.size() - pos }
			};
			
			check();
		}
		sample(sample&& rhs) noexcept:
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
				BOOST_ASSERT(prev.empty() || prev.data() + prev.size() == c.data());
				prev = c;
			}
		}

		std::string request;
		std::deque<string_view> chunks;
		std::deque<GoodTestCase> cases;
	};

	struct iterator
	{
		explicit iterator(std::deque<GoodTestCase> cases) :
			cases{ move(cases) } {}

		sample operator*() const
		{
			return { cases, pos };
		}

		iterator& operator++()
		{
			++pos;
			return *this;
		}

	private:
		std::deque<GoodTestCase> cases;
		std::size_t pos = 1;
	};

	enum { arity = 1 };

	boost::unit_test::data::size_t size() const
	{
		std::size_t size = 0;
		for (auto& t : cases)
			size += t.request.size();
		return size - 1;
	}

	iterator begin() const { return iterator{ cases }; }

private:
	std::deque<GoodTestCase> cases = {
		good_request_samples.at(0),
		good_request_samples.at(1),
		good_request_samples.at(3),
	};
};

namespace boost::unit_test::data::monomorphic
{
template <>
struct is_dataset<fragmented_pipeline_dataset> : mpl::true_ {};
}

static std::ostream& operator<<(std::ostream& s,
	const fragmented_pipeline_dataset::sample& ds)
{
	s << "[";
	for (auto& c : ds.chunks)
		s << "\"" << c << "\" ";
	s << "]";
	return s;
}

BOOST_FIXTURE_TEST_SUITE(parser_tests, ParserFixture)

BOOST_DATA_TEST_CASE(test_simple,
	boost::unit_test::data::make(good_request_samples)
	+ boost::unit_test::data::make(good_last_request_samples))
{
	auto result = p.parse_chunk(sample.request);
	Parser::finalize(req);

	auto error = std::get_if<Error>(&result);
	if (error)
		BOOST_TEST_MESSAGE(std::to_string(static_cast<int>(error->code))
			+ " " + std::string{ error->details });
	BOOST_TEST_REQUIRE(!error);

	auto r = std::get_if<Parser::CompleteRequest>(&result);
	BOOST_TEST_REQUIRE(r);
	BOOST_TEST(r->rest.empty());
	BOOST_TEST(req.method.type == sample.method_type);
	BOOST_TEST(req.method.name == sample.method_name);
	BOOST_TEST(req.url.path == sample.url);
	BOOST_TEST(req.http_version == sample.version);
	BOOST_TEST(req.headers == sample.lowercased_headers(), boost::test_tools::per_element());
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

		auto result_error = std::get_if<Error>(&result);
		BOOST_TEST(!result_error);

		if (auto result_complete = std::get_if<Parser::CompleteRequest>(&result))
		{
			Parser::finalize(req);

			if (!result_complete->rest.empty())
				chunks.emplace_front(result_complete->rest);

			BOOST_TEST(!cases.empty());
			auto expected = cases.front();
			cases.pop_front();

			BOOST_TEST(req.method.type == expected.method_type);
			BOOST_TEST(req.method.name == expected.method_name);
			BOOST_TEST(req.url.path == expected.url);
			BOOST_TEST(req.http_version == expected.version);
			BOOST_TEST(req.headers == expected.lowercased_headers(), boost::test_tools::per_element());
			BOOST_TEST(body(req) == expected.body);

			reset();
		}
	}

	BOOST_TEST(cases.empty());
}

BOOST_DATA_TEST_CASE(test_errors, boost::unit_test::data::make(bad_request_samples))
{
	auto result = p.parse_chunk(sample.request);
	Parser::finalize(req);

	auto r = std::get_if<Error>(&result);
	BOOST_TEST_REQUIRE(r);
	BOOST_TEST(r->code == sample.code.value_or(Response::Status::bad_request));
}

BOOST_AUTO_TEST_SUITE_END()