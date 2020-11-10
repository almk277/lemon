#pragma once
#include "string_view.hpp"
#include "arena.hpp"
#include <iosfwd>
#include <iterator>
#include <list>

struct Url
{
	string_view all;
	string_view path;
	string_view query;
};

namespace http
{
struct Message
{
	struct Header
	{
		using lowercase_string_view = string_view;

		string_view name;
		lowercase_string_view lowercase_name;
		string_view value;

		constexpr Header(string_view name, string_view value) noexcept : name{name}, value{value} {}

		bool operator==(const Header& rhs) const noexcept
		{
			return lowercase_name == rhs.lowercase_name && value == rhs.value;
		}

		[[nodiscard]] bool is(lowercase_string_view name) const noexcept
		{
			return lowercase_name == name;
		}

		[[nodiscard]] static auto make_is(lowercase_string_view name) noexcept
		{
			return [name](const Header& hdr) { return hdr.is(name); };
		}

		static constexpr string_view sep = ": "sv;
	};

	enum class ProtocolVersion
	{
		http_1_0 = 0,
		http_1_1 = 1,
	};

	using HeaderList = std::list<Header, Arena::Allocator<Header>>;
	using ChunkList = std::list<string_view, Arena::Allocator<string_view>>;

	explicit Message(Arena& a) noexcept :
	    http_version{},
	    headers{a.make_allocator<Header>("message::headers")},
	    body{a.make_allocator<string_view>("message::body")},
	    a{a}
	{}

	Message(const Message&) = delete;
	Message(Message&&) = delete;
	Message& operator=(const Message&) = delete;
	Message& operator=(Message&&) = delete;
	~Message() = default;

	// HTTP new line
	static constexpr string_view nl = "\r\n"sv;

	ProtocolVersion http_version;
	HeaderList headers;
	ChunkList body;

	Arena& a;
};

struct Request : Message
{
	struct Method
	{
		enum class Type
		{
			get,
			head,
			post,
			other
		};
		Type type = Type::other;
		string_view name;
	};

	explicit Request(Arena& a) noexcept : Message{a} {}

	Method method;
	Url url;
	bool keep_alive = false;
	std::size_t content_length = 0;
};

struct Response : Message
{
	enum class Status;
	class const_iterator;

	explicit Response(Arena& a) noexcept;

	Status code;

	const_iterator begin() const noexcept;
	const_iterator cbegin() const noexcept;
	const_iterator end() const noexcept;
	const_iterator cend() const noexcept;
};

enum class Response::Status
{
	continue_ = 100,
	switching_protocols = 101,
	processing = 102,

	ok = 200,
	created = 201,
	accepted = 202,
	non_authoritative_information = 203,
	no_content = 204,
	reset_content = 205,
	partial_content = 206,
	multi_status = 207,
	already_reported = 208,
	im_used = 226,

	multiple_choices = 300,
	moved_permanently = 301,
	found = 302,
	see_other = 303,
	not_modified = 304,
	use_proxy = 305,
	switch_proxy = 306,
	temporary_redirect = 307,
	permanent_redirect = 308,

	bad_request = 400,
	unauthorized = 401,
	payment_required = 402,
	forbidden = 403,
	not_found = 404,
	method_not_allowed = 405,
	not_acceptable = 406,
	proxy_authentication_required = 407,
	request_timeout = 408,
	conflict = 409,
	gone = 410,
	length_required = 411,
	precondition_failed = 412,
	payload_too_large = 413,
	uri_too_long = 414,
	unsupported_media_type = 415,
	range_not_satisfiable = 416,
	expectation_failed = 417,
	im_a_teapot = 418,
	misdirected_request = 421,
	unprocessable_entity = 422,
	locked = 423,
	failed_dependency = 424,
	upgrade_required = 426,
	precondition_required = 428,
	too_many_requests = 429,
	request_header_fields_too_large = 431,
	unavailable_for_legal_reasons = 451,

	internal_server_error = 500,
	not_implemented = 501,
	bad_gateway = 502,
	service_unavailable = 503,
	gateway_timeout = 504,
	http_version_not_supported = 505,
	variant_also_negotiates = 506,
	insufficient_storage = 507,
	loop_detected = 508,
	not_extended = 510,
	network_authentication_required = 511,
};

class Response::const_iterator
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
	const_iterator(const Response* r, begin_tag) noexcept;
	const_iterator(const Response* r, end_tag) noexcept;
	const_iterator(const const_iterator&) = default;
	const_iterator(const_iterator&&) = default;
	~const_iterator() = default;

	auto operator=(const const_iterator&) -> const_iterator& = default;
	auto operator=(const_iterator&&) -> const_iterator& = default;

	auto operator*() const -> reference;
	auto operator-> () const -> pointer;
	auto operator++() -> const_iterator&;
	auto operator++(int) -> const_iterator;
	auto operator--() -> const_iterator&;
	auto operator--(int) -> const_iterator;

	friend auto operator==(const const_iterator& it1, const const_iterator& it2) -> bool;
	friend auto operator!=(const const_iterator& it1, const const_iterator& it2) -> bool;

private:
	enum class State;

	const Response* r;
	State s;
	HeaderList::const_iterator header_it;
	ChunkList::const_iterator body_it;
};

inline Response::Response(Arena& a) noexcept : Message{a}, code{Status::internal_server_error} {}

string_view to_string(Response::Status status);

std::ostream& operator<<(std::ostream& stream, Response::Status status);

auto calc_content_length(const Message& msg) noexcept -> std::size_t;
}