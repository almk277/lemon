#pragma once
#include "string_view.hpp"
#include "http_error.hpp"
#include "http_parser.h"
#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>
#include <utility>
#include <variant>

namespace http
{
struct Request;
	
class Parser: boost::noncopyable
{
public:
	struct RequestLine {};
	struct IncompleteRequest {};
	struct CompleteRequest {};
	using Result = std::pair<std::variant<Error, RequestLine, IncompleteRequest, CompleteRequest>, string_view>;

	Parser() = default;

	auto reset(Request& req, bool& drop_mode) noexcept -> void;
	auto parse_chunk(string_view chunk) noexcept -> Result;
	auto finalize(Request& req) const -> void;

protected:
	enum class State {
		start,
		request_line,
		body
	};
	
	enum class HeaderState { key, value };
	
	struct Context
	{
		Request* r;
		State state;
		HeaderState hdr_state;
		boost::optional<Error> error;
		bool* drop_mode;
	};

	//TODO pImpl
	http_parser p;
	Context ctx;
};
}