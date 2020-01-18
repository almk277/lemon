#pragma once

#include "string_view.hpp"
#include "message.hpp"
#include <stdexcept>

struct http_error
{
	explicit http_error(response::status code,
		string_view details = {}) noexcept:
		code{code},
		details{details}
	{}
	http_error(const http_error&) noexcept = default;
	http_error(http_error&&) noexcept = default;
	~http_error() = default;
	http_error &operator=(const http_error&) noexcept = default;
	http_error &operator=(http_error&&) noexcept = default;

	response::status code;
	string_view details;
};

class http_exception: public std::runtime_error
{
public:
	explicit http_exception(response::status code, string_view details = {}):
		runtime_error{ "HTTP error" },
		error{code, details}
	{}

	explicit http_exception(string_view details):
		http_exception{ response::status::INTERNAL_SERVER_ERROR, details} {}

	template <int N>
	explicit http_exception(const char (&details)[N]):
		runtime_error{details},
		error{ response::status::INTERNAL_SERVER_ERROR,
			string_view{details, N} }
	{}

	response::status status_code() const noexcept { return error.code; }
	string_view detail_string() const noexcept { return error.details;  }
private:
	const http_error error;
};