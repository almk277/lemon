#include "message.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <boost/range/numeric.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <array>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <cstdlib>
#include <string>

using namespace std::string_literals;

namespace
{
BOOST_CONCEPT_ASSERT((boost::BidirectionalIterator<response::const_iterator>));

const string_view &to_string_ref(message::http_version_type v)
{
	static constexpr std::array strings = {
		"HTTP/1.0 "sv,
		"HTTP/1.1 "sv
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

constexpr string_view m0[] = { ""sv };

constexpr string_view m1[] = {
	"100 Continue"sv,
	"101 Switching Protocols"sv,
	"102 Processing"sv,
};

constexpr string_view m2[] = {
	"200 OK"sv,
	"201 Created"sv,
	"202 Accepted"sv,
	"203 Non-Authoritative Information"sv,
	"204 No Content"sv,
	"205 Reset Content"sv,
	"206 Partial Content"sv,
	"207 Multi-Status"sv,
	"208 Already Reported"sv,
};

constexpr string_view m3[] = {
	"300 Multiple Choices"sv,
	"301 Moved Permanently"sv,
	"302 Found"sv,
	"303 See Other"sv,
	"304 Not Modified"sv,
	"305 Use Proxy"sv,
	"306 Switch Proxy"sv,
	"307 Temporary Redirect"sv,
	"308 Permanent Redirect"sv,
};

constexpr string_view m4[] = {
	"400 Bad Request"sv,
	"401 Unauthorized"sv,
	"402 Payment Required"sv,
	"403 Forbidden"sv,
	"404 Not Found"sv,
	"405 Method Not Allowed"sv,
	"406 Not Acceptable"sv,
	"407 Proxy Authentication Required"sv,
	"408 Request Timeout"sv,
	"409 Conflict"sv,
	"410 Gone"sv,
	"411 Length Required"sv,
	"412 Precondition Failed"sv,
	"413 Payload Too Large"sv,
	"414 URI Too Long"sv,
	"415 Unsupported Media Type"sv,
	"416 Range Not Satisfiable"sv,
	"417 Expectation Failed"sv,
	"418 I'm a teapot"sv,
	"419 419"sv,
	"420 420"sv,
	"421 Misdirected Request"sv,
	"422 Unprocessable Entity"sv,
	"423 Locked"sv,
	"424 Failed Dependency"sv,
	"425 425"sv,
	"426 Upgrade Required"sv,
	"427 427"sv,
	"428 Precondition Required"sv,
	"429 Too Many Requests"sv,
	"430 430"sv,
	"431 Request Header Fields Too Large"sv,
};

constexpr string_view m5[] = {
	"500 Internal Server Error"sv,
	"501 Not Implemented"sv,
	"502 Bad Gateway"sv,
	"503 Service Unavailable"sv,
	"504 Gateway Timeout"sv,
	"505 HTTP Version Not Supported"sv,
	"506 Variant Also Negotiates"sv,
	"507 Insufficient Storage"sv,
	"508 Loop Detected"sv,
	"509 509"sv,
	"510 Not Extended"sv,
	"511 Network Authentication Required"sv,
};

constexpr string_list strings[] = {
	m0, m1, m2, m3, m4, m5
};

[[noreturn]] void bad_response(int n)
{
	throw std::runtime_error{ "bad response code: "s + std::to_string(n) };
}

const string_view &to_string_other(int status)
{
	static const std::unordered_map<int, string_view> string_map = {
		{ 226, "226 IM Used"sv },
		{ 451, "451 Unavailable For Legal Reasons"sv },
	};

	auto status_it = string_map.find(status);
	if (status_it == string_map.end())
		bad_response(status);
	return status_it->second;
}

const string_view &to_string_ref(response::status status)
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
}

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

string_view to_string(response::status status)
{
	return to_string_ref(status);
}

std::ostream &operator<<(std::ostream &stream, response::status status)
{
	return stream << static_cast<int>(status);
}

auto calc_content_length(const message &msg) noexcept -> std::size_t
{
	auto length = accumulate(
		msg.body | boost::adaptors::transformed(mem_fn(&string_view::size)),
		decltype(msg.body.front().size()){});
	return length;
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
	case state::HEADER_NAME: [[fallthrough]];
	case state::HEADER_NAME_SEP: [[fallthrough]];
	case state::HEADER_VAL: [[fallthrough]];
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