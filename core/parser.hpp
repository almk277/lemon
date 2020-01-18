#pragma once

#include "string_view.hpp"
#include "http_parser.h"
#include "http_error.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/variant/variant.hpp>
#include <boost/optional/optional.hpp>
struct request;
class arena;

class parser: boost::noncopyable
{
public:
	struct incomplete_request {};
	struct complete_request
	{
		string_view rest;
	};
	using result = boost::variant<http_error, incomplete_request, complete_request>;

	parser() = default;
	~parser() = default;

	void reset(request &req, arena &a) noexcept;
	result parse_chunk(string_view chunk) noexcept;

protected:
	struct context
	{
		enum class hdr { KEY, VAL };

		request *r;
		arena *a;
		hdr hdr_state;
		boost::optional<http_error> error;
		bool comp;
	};

	//TODO pImpl
	http_parser p;
	context ctx;
};