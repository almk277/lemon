#pragma once

#include "string_view.hpp"
#include "http_parser.h"
#include "http_error.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>
#include <variant>
struct request;

class parser: boost::noncopyable
{
public:
	struct incomplete_request {};
	struct complete_request
	{
		string_view rest;
	};
	using result = std::variant<http_error, incomplete_request, complete_request>;

	parser() = default;

	void reset(request &req) noexcept;
	result parse_chunk(string_view chunk) noexcept;

	static void finalize(request &req);

protected:
	struct context
	{
		enum class hdr { KEY, VAL };

		request *r;
		hdr hdr_state;
		boost::optional<http_error> error;
		bool complete;
	};

	//TODO pImpl
	http_parser p;
	context ctx;
};