#pragma once

#include "string_view.hpp"
#include "arena.hpp"
#include <ostream>
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
		using lowercase_string_view = string_view;

		string_view name;
		lowercase_string_view lowercase_name;
		string_view value;

		explicit header(string_view name) noexcept : name{name} {}
		constexpr header(string_view name, string_view value) noexcept : name{name}, value{value} {}

		bool operator==(const header &rhs) const noexcept
		{
			return lowercase_name == rhs.lowercase_name && value == rhs.value;
		}

		[[nodiscard]] bool is(lowercase_string_view name) const noexcept
		{
			return lowercase_name == name;
		}

		[[nodiscard]] static auto make_is(lowercase_string_view name) noexcept
		{
			return [name](const header &hdr) { return hdr.is(name); };
		}

		static constexpr string_view SEP = ": "sv;
	};

	enum class http_version_type
	{
		HTTP_1_0 = 0,
		HTTP_1_1 = 1,
	};

	using header_list = std::list<header, arena::allocator<header>>;
	using chunk_list = std::list<string_view, arena::allocator<string_view>>;

	explicit message(arena &a) noexcept :
	    http_version{},
	    headers{a.make_allocator<header>("message::headers")},
	    body{a.make_allocator<string_view>("message::body")},
	    a{a}
	{}

	message(const message &) = delete;
	message(message &&) = delete;
	message &operator=(const message &) = delete;
	message &operator=(message &&) = delete;
	~message() = default;

	// HTTP new line
	static constexpr string_view NL = "\r\n"sv;

	http_version_type http_version;
	header_list headers;
	chunk_list body;

	arena &a;
};

struct request : message
{
	struct method_s
	{
		enum class type_e
		{
			GET,
			HEAD,
			POST,
			OTHER
		};
		type_e type = type_e::OTHER;
		string_view name;
	};

	explicit request(arena &a) noexcept : message{a} {}

	method_s method;
	url_s url;
	bool keep_alive = false;
	std::size_t content_length = 0;
};

struct response : message
{
	enum class status;
	class const_iterator;

	explicit response(arena &a) noexcept;

	status code;

	const_iterator begin() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cend() const noexcept;
};

enum class response::status
{
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,
	PROCESSING = 102,

	OK = 200,
	CREATED = 201,
	ACCEPTED = 202,
	NON_AUTHORITATIVE_INFORMATION = 203,
	NO_CONTENT = 204,
	RESET_CONTENT = 205,
	PARTIAL_CONTENT = 206,
	MULTI_STATUS = 207,
	ALREADY_REPORTED = 208,
	IM_USED = 226,

	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,
	NOT_MODIFIED = 304,
	USE_PROXY = 305,
	SWITCH_PROXY = 306,
	TEMPORARY_REDIRECT = 307,
	PERMANENT_REDIRECT = 308,

	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,
	NOT_ACCEPTABLE = 406,
	PROXY_AUTHENTICATION_REQUIRED = 407,
	REQUEST_TIMEOUT = 408,
	CONFLICT = 409,
	GONE = 410,
	LENGTH_REQUIRED = 411,
	PRECONDITION_FAILED = 412,
	PAYLOAD_TOO_LARGE = 413,
	URI_TOO_LONG = 414,
	UNSUPPORTED_MEDIA_TYPE = 415,
	RANGE_NOT_SATISFIABLE = 416,
	EXPECTATION_FAILED = 417,
	IM_A_TEAPOT = 418,
	MISDIRECTED_REQUEST = 421,
	UNPROCESSABLE_ENTITY = 422,
	LOCKED = 423,
	FAILED_DEPENDENCY = 424,
	UPGRADE_REQUIRED = 426,
	PRECONDITION_REQUIRED = 428,
	TOO_MANY_REQUESTS = 429,
	REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
	VARIANT_ALSO_NEGOTIATES = 506,
	INSUFFICIENT_STORAGE = 507,
	LOOP_DETECTED = 508,
	NOT_EXTENDED = 510,
	NETWORK_AUTHENTICATION_REQUIRED = 511,
};

class response::const_iterator
{
public:
	using value_type = string_view;
	using reference = const value_type &;
	using pointer = const value_type *;
	using difference_type = int;
	using iterator_category = std::bidirectional_iterator_tag;

	struct begin_tag {};
	struct end_tag {};

	const_iterator() noexcept;
	const_iterator(const response *r, begin_tag) noexcept;
	const_iterator(const response *r, end_tag) noexcept;
	const_iterator(const const_iterator &) = default;
	const_iterator(const_iterator &&) = default;
	~const_iterator() = default;

	auto operator=(const const_iterator &) -> const_iterator & = default;
	auto operator=(const_iterator &&) -> const_iterator & = default;

	auto operator*() const -> reference;
	auto operator-> () const -> pointer;
	auto operator++() -> const_iterator &;
	auto operator++(int) -> const_iterator;
	auto operator--() -> const_iterator &;
	auto operator--(int) -> const_iterator;

	friend auto operator==(const const_iterator &it1, const const_iterator &it2) -> bool;
	friend auto operator!=(const const_iterator &it1, const const_iterator &it2) -> bool;

private:
	enum class state;

	const response *r;
	state s;
	header_list::const_iterator header_it;
	chunk_list::const_iterator body_it;
};

inline response::response(arena &a) noexcept : message{a}, code{status::INTERNAL_SERVER_ERROR} {}

string_view to_string(response::status status);

std::ostream &operator<<(std::ostream &stream, response::status status);
