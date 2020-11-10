#pragma once
#include "string_view.hpp"
#include "http_error.hpp"
#include "http_parser.h"
#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>
#include <variant>

namespace http
{
struct Request;
	
class Parser: boost::noncopyable
{
public:
	struct IncompleteRequest {};
	struct CompleteRequest
	{
		string_view rest;
	};
	using Result = std::variant<Error, IncompleteRequest, CompleteRequest>;

	Parser() = default;

	void reset(Request& req) noexcept;
	Result parse_chunk(string_view chunk) noexcept;

	static void finalize(Request& req);

protected:
	struct Context
	{
		enum class HeaderState { KEY, VAL };

		Request* r;
		HeaderState hdr_state;
		boost::optional<Error> error;
		bool complete;
	};

	//TODO pImpl
	http_parser p;
	Context ctx;
};
}