#pragma once

#include "utility.hpp"
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
	using buffer = string_view;
	struct incomplete_request {};
	struct complete_request
	{
		buffer rest;
	};
	using result = boost::variant<http_error, incomplete_request, complete_request>;

	parser() = default;
	~parser() = default;

	void reset(request &req, arena &a) noexcept;
	result parse_chunk(buffer buf) noexcept;

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

	http_parser p;
	context ctx;
};