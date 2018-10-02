#include "message.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <array>
#include <unordered_map>
#include <stdexcept>
#include <cstdlib>

using namespace std::string_literals;

BOOST_CONCEPT_ASSERT((boost::BidirectionalIterator<response::const_iterator>));

constexpr string_view message::NL;
constexpr string_view message::header::SEP;

auto response::begin() const noexcept -> const_iterator
{
	return { this, const_iterator::begin_tag{} };
}

auto response::cbegin() const noexcept -> const_iterator
{
	return begin();
}

auto response::end() const noexcept -> const_iterator
{
	return { this, const_iterator::end_tag{} };
}

auto response::cend() const noexcept -> const_iterator
{
	return end();
}

static const string_view &to_string_ref(message::http_version_type v)
{
	static const std::array<string_view, 2> strings = {
		"HTTP/1.0 "_w,
		"HTTP/1.1 "_w
	};

	auto idx = static_cast<std::size_t>(v);
	BOOST_ASSERT(idx < strings.size());
	return strings[idx];
}

struct string_list
{
	const string_view *data;
	int size;

	template <int N>
	constexpr string_list(const string_view(&a)[N]) :
		data{a},
		size{N}
	{}
};

constexpr string_view m0[] = { ""_w };

constexpr string_view m1[] = {
	"100 Continue"_w,
	"101 Switching Protocols"_w,
	"102 Processing"_w,
};

constexpr string_view m2[] = {
	"200 OK"_w,
	"201 Created"_w,
	"202 Accepted"_w,
	"203 Non-Authoritative Information"_w,
	"204 No Content"_w,
	"205 Reset Content"_w,
	"206 Partial Content"_w,
	"207 Multi-Status"_w,
	"208 Already Reported"_w,
};

constexpr string_view m3[] = {
	"300 Multiple Choices"_w,
	"301 Moved Permanently"_w,
	"302 Found"_w,
	"303 See Other"_w,
	"304 Not Modified"_w,
	"305 Use Proxy"_w,
	"306 Switch Proxy"_w,
	"307 Temporary Redirect"_w,
	"308 Permanent Redirect"_w,
};

constexpr string_view m4[] = {
	"400 Bad Request"_w,
	"401 Unauthorized"_w,
	"402 Payment Required"_w,
	"403 Forbidden"_w,
	"404 Not Found"_w,
	"405 Method Not Allowed"_w,
	"406 Not Acceptable"_w,
	"407 Proxy Authentication Required"_w,
	"408 Request Timeout"_w,
	"409 Conflict"_w,
	"410 Gone"_w,
	"411 Length Required"_w,
	"412 Precondition Failed"_w,
	"413 Payload Too Large"_w,
	"414 URI Too Long"_w,
	"415 Unsupported Media Type"_w,
	"416 Range Not Satisfiable"_w,
	"417 Expectation Failed"_w,
	"418 I'm a teapot"_w,
	"419 419"_w,
	"420 420"_w,
	"421 Misdirected Request"_w,
	"422 Unprocessable Entity"_w,
	"423 Locked"_w,
	"424 Failed Dependency"_w,
	"425 425"_w,
	"426 Upgrade Required"_w,
	"427 427"_w,
	"428 Precondition Required"_w,
	"429 Too Many Requests"_w,
	"430 430"_w,
	"431 Request Header Fields Too Large"_w,
};

constexpr string_view m5[] = {
	"500 Internal Server Error"_w,
	"501 Not Implemented"_w,
	"502 Bad Gateway"_w,
	"503 Service Unavailable"_w,
	"504 Gateway Timeout"_w,
	"505 HTTP Version Not Supported"_w,
	"506 Variant Also Negotiates"_w,
	"507 Insufficient Storage"_w,
	"508 Loop Detected"_w,
	"509 509"_w,
	"510 Not Extended"_w,
	"511 Network Authentication Required"_w,
};

constexpr string_list strings[] = {
	m0, m1, m2, m3, m4, m5
};

[[noreturn]] static void bad_response(int n)
{
	throw std::runtime_error{ "bad response code: "s + std::to_string(n) };
}

static const string_view &to_string_other(int status)
{
	static const std::unordered_map<int, string_view> string_map = {
		{ 226, "226 IM Used"_w },
		{ 451, "451 Unavailable For Legal Reasons"_w },
	};

	auto status_it = string_map.find(status);
	if (status_it == string_map.end())
		bad_response(status);
	return status_it->second;
}

static const string_view &to_string_ref(response::status status)
{
	const auto status_n = static_cast<int>(status);
	auto dv = std::div(status_n, 100);

	auto group = dv.quot;
	if (BOOST_UNLIKELY(group < 1 || group > 5))
		bad_response(status_n);

	auto &sublist = strings[group];
	auto subcode = dv.rem;
	if (BOOST_LIKELY(subcode < sublist.size))
		return sublist.data[subcode];

	return to_string_other(status_n);
}

string_view to_string(response::status status)
{
	return to_string_ref(status);
}

enum class response::const_iterator::state
{
	HTTP_VERSION,
	STATUS,
	STATUS_NL,
	HEADER_NAME,
	HEADER_NAME_SEP,
	HEADER_VAL,
	HEADER_VAL_NL,
	HEADER_NL,
	BODY,
	END
};

response::const_iterator::const_iterator() noexcept:
	r{ nullptr },
	s{ state::END }
{}

response::const_iterator::const_iterator(const response *r, begin_tag) noexcept :
	r{ r },
	s{ state::HTTP_VERSION }
{}

response::const_iterator::const_iterator(const response *r, end_tag) noexcept :
	r{ r },
	s{ state::END }
{}

auto response::const_iterator::operator*() const -> reference
{
	switch (s)
	{
	case state::HTTP_VERSION:
		return to_string_ref(r->http_version);
	case state::STATUS:
		return to_string_ref(r->code);
	case state::STATUS_NL:
		return NL;
	case state::HEADER_NAME:
		return header_it->name;
	case state::HEADER_NAME_SEP:
		return header::SEP;
	case state::HEADER_VAL:
		return header_it->value;
	case state::HEADER_VAL_NL:
		return NL;
	case state::HEADER_NL:
		return NL;
	case state::BODY:
		return *body_it;
	case state::END:
		break;
	}

	throw std::logic_error{ "response::const_iterator::operator*: non-dereferenceable" };
}

auto response::const_iterator::operator->() const -> pointer
{
	return &**this;
}

auto response::const_iterator::operator++() -> const_iterator&
{
	switch (s)
	{
	case state::HTTP_VERSION:
		s = state::STATUS;
		break;
	case state::STATUS:
		s = state::STATUS_NL;
		break;
	case state::STATUS_NL:
		if (!r->headers.empty()) {
			s = state::HEADER_NAME;
			header_it = r->headers.begin();
		} else {
			s = state::HEADER_NL;
		}
		break;
	case state::HEADER_NAME:
		s = state::HEADER_NAME_SEP;
		break;
	case state::HEADER_NAME_SEP:
		s = state::HEADER_VAL;
		break;
	case state::HEADER_VAL:
		s = state::HEADER_VAL_NL;
		break;
	case state::HEADER_VAL_NL:
		++header_it;
		if (header_it != r->headers.end())
			s = state::HEADER_NAME;
		else
			s = state::HEADER_NL;
		break;
	case state::HEADER_NL:
		if (!r->body.empty()) {
			s = state::BODY;
			body_it = r->body.begin();
		} else {
			s = state::END;
		}
		break;
	case state::BODY:
		++body_it;
		if (body_it == r->body.end())
			s = state::END;
		break;
	case state::END:
		throw std::logic_error{ "response::const_iterator::operator++: non-incrementable" };
	}

	return *this;
}

auto response::const_iterator::operator++(int) -> const_iterator
{
	auto tmp{ *this };
	++*this;
	return tmp;
}

auto response::const_iterator::operator--() -> const_iterator&
{
	switch (s)
	{
	case state::HTTP_VERSION:
		throw std::logic_error{ "response::const_iterator::operator--: non-decrementable" };
	case state::STATUS:
		s = state::HTTP_VERSION;
		break;
	case state::STATUS_NL:
		s = state::STATUS;
		break;
	case state::HEADER_NAME:
		if (header_it == r->headers.begin()) {
			s = state::STATUS_NL;
		} else {
			--header_it;
			s = state::HEADER_VAL_NL;
		}
		break;
	case state::HEADER_NAME_SEP:
		s = state::HEADER_NAME;
		break;
	case state::HEADER_VAL:
		s = state::HEADER_NAME_SEP;
		break;
	case state::HEADER_VAL_NL:
		s = state::HEADER_VAL;
		break;
	case state::HEADER_NL:
		if (!r->headers.empty()) {
			s = state::HEADER_VAL_NL;
			header_it = --r->headers.end();
		} else {
			s = state::STATUS_NL;
		}
		break;
	case state::BODY:
		if (body_it != r->body.begin())
			--body_it;
		else
			s = state::HEADER_NL;
		break;
	case state::END:
		if (!r->body.empty()) {
			s = state::BODY;
			body_it = --r->body.end();
		} else {
			s = state::HEADER_NL;
		}
		break;
	}

	return *this;
}

auto response::const_iterator::operator--(int) -> const_iterator
{
	auto tmp{ *this };
	--*this;
	return tmp;
}

auto operator==(const response::const_iterator& it1,
	const response::const_iterator& it2) -> bool
{
	if (BOOST_UNLIKELY(it1.r != it2.r))
		throw std::logic_error{ "response::const_iterator::operator==: "
			"comparing different containers iterators" };

	if (it1.s != it2.s)
		return false;

	switch (it1.s)
	{
	using state = response::const_iterator::state;
	case state::HEADER_NAME: BOOST_FALLTHROUGH;
	case state::HEADER_NAME_SEP: BOOST_FALLTHROUGH;
	case state::HEADER_VAL: BOOST_FALLTHROUGH;
	case state::HEADER_VAL_NL:
		return it1.header_it == it2.header_it;
	case state::BODY:
		return it1.body_it == it2.body_it;
	default:
		return true;
	}
}

auto operator!=(const response::const_iterator& it1,
	const response::const_iterator& it2) -> bool
{
	return !(it1 == it2);
}