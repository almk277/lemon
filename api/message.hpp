#pragma once

#include "utility.hpp"
#include "response_status.hpp"
#include "arena.hpp"
#include <list>

struct url_s
{
	string_view all;
	string_view path;
	string_view query;
};

struct message: noncopyable
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
};