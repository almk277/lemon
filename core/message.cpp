#include "message.hpp"
#include <boost/assert.hpp>
#include <boost/concept_check.hpp>
#include <array>
#include <stdexcept>

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

static const string_view &http_version_string(message::http_version_type v)
{
	static const std::array<string_view, 2> strings = {
		"HTTP/1.0 "_w,
		"HTTP/1.1 "_w
	};

	auto idx = static_cast<std::size_t>(v);
	BOOST_ASSERT(idx < strings.size());
	return strings[idx];
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
		return http_version_string(r->http_version);
	case state::STATUS:
		return response_status_string(r->code);
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
	BOOST_ASSERT(it1.r == it2.r);

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