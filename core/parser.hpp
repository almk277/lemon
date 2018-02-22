#pragma once

#include "utility.hpp"
#include "http_parser.h"
#include "http_error.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/variant/variant.hpp>
#include <boost/optional/optional.hpp>
struct request;
class arena;

class parser: noncopyable
{
public:
	using buffer = boost::asio::const_buffer;
	using result = boost::variant<buffer, http_error>;

	parser() = default;
	~parser() = default;

	void reset(request &req, arena &a) noexcept;
	result parse_chunk(buffer buf) noexcept;
	bool complete() const noexcept { return ctx.comp; }

protected:
	struct context {
		request *r;
		arena *a;
		boost::optional<http_error> error;
		bool comp;
	};

	http_parser p;
	context ctx;
};