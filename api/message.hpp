#pragma once

#include "utility.hpp"
#include "response_status.hpp"
#include "arena.hpp"
#include <iterator>
#include <list>

struct url_s
{
	string_view all;
	string_view path;
	string_view query;
};

struct message
{
	struct header
	{
		string_view name;
		string_view lcase_name;
		string_view value;

		explicit header(string_view name) noexcept:
			name{ name }, lcase_name{ name }
		{}
		constexpr header(string_view name, string_view value) noexcept:
			name{ name }, lcase_name{ name }, value{ value }
		{}

		bool operator==(const header &rhs) const noexcept
		{
			return lcase_name == rhs.lcase_name
				&& value == rhs.value;
		}

		static constexpr string_view SEP = ": "_w;
	};

	enum class http_version_type {
		HTTP_1_0 = 0,
		HTTP_1_1 = 1,
	};

	using header_list = std::list<header, arena::allocator<header>>;
	using chunk_list = std::list < string_view, arena::allocator<string_view>>;

	explicit message(arena &a) noexcept:
		http_version{},
		headers{a.make_allocator<header>("message::headers")},
		body{a.make_allocator<string_view>("message::body")},
		a{a}
	{}

	message(const message&) = delete;
	message(message&&) = delete;
	message &operator=(const message&) = delete;
	message &operator=(message&&) = delete;

	// HTTP new line
	static constexpr string_view NL = "\r\n"_w;

	http_version_type http_version;
	header_list headers;
	chunk_list body;

	arena &a;
};

struct request: message
{
	struct method_s
	{
		enum class type_e {
			GET,
			HEAD,
			POST,
			OTHER
		};
		type_e type;
		string_view name;
	};

	explicit request(arena &a) noexcept: message{ a } {}

	method_s method;
	url_s url;
	bool keep_alive = false;
	std::size_t content_length = 0;
};

struct response: message
{
	explicit response(arena &a) noexcept: message{ a } {}

	response_status code = response_status::INTERNAL_SERVER_ERROR;

	class const_iterator;

	const_iterator begin() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cend() const noexcept;
};

class response::const_iterator
{
public:
	using value_type = string_view;
	using reference = const value_type&;
	using pointer = const value_type*;
	using difference_type = int;
	using iterator_category = std::bidirectional_iterator_tag;

	struct begin_tag {};
	struct end_tag {};

	const_iterator() noexcept;
	const_iterator(const response *r, begin_tag) noexcept;
	const_iterator(const response *r, end_tag) noexcept;
	const_iterator(const const_iterator&) = default;
	const_iterator(const_iterator&&) = default;
	~const_iterator() = default;

	auto operator=(const const_iterator&) -> const_iterator& = default;
	auto operator=(const_iterator&&) -> const_iterator& = default;

	auto operator*() const -> reference;
	auto operator->() const -> pointer;
	auto operator++() -> const_iterator&;
	auto operator++(int) -> const_iterator;
	auto operator--() -> const_iterator&;
	auto operator--(int) -> const_iterator;

	friend auto operator==(const const_iterator& it1,
		const const_iterator& it2) -> bool;
	friend auto operator!=(const const_iterator& it1,
		const const_iterator& it2) -> bool;

private:
	enum class state;

	const response *r;
	state s;
	header_list::const_iterator header_it;
	chunk_list::const_iterator body_it;
};
