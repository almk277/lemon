#pragma once

#include "utility.hpp"
#include "response_status.hpp"
#include <stdexcept>

struct http_error
{
	explicit http_error(response_status code,
		string_view details = {}) noexcept:
		code{code},
		details{details}
	{}
	http_error(const http_error&) noexcept = default;
	http_error(http_error&&) noexcept = default;
	~http_error() = default;
	http_error &operator=(const http_error&) noexcept = default;
	http_error &operator=(http_error&&) noexcept = default;

	response_status code;
	string_view details;
};

class http_exception: public std::runtime_error
{
public:
	explicit http_exception(response_status code, string_view details = {}):
		runtime_error{ "HTTP error" },
		error{code, details}
	{}

	explicit http_exception(string_view details):
		http_exception{response_status::INTERNAL_SERVER_ERROR, details} {}

	template <int N>
	explicit http_exception(const char (&details)[N]):
		runtime_error{details},
		error{ response_status::INTERNAL_SERVER_ERROR,
			string_view{details, N} }
	{}

	~http_exception() = default;

	response_status status_code() const noexcept { return error.code; }
	string_view detail_string() const noexcept { return error.details;  }
private:
	const http_error error;
};