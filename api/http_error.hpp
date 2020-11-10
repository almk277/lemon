#pragma once
#include "string_view.hpp"
#include "http_message.hpp"
#include <stdexcept>

namespace http
{
struct Error
{
	explicit Error(Response::Status code,
		string_view details = {}) noexcept:
		code{code},
		details{details}
	{}
	Error(const Error&) noexcept = default;
	Error(Error&&) noexcept = default;
	~Error() = default;
	Error& operator=(const Error&) noexcept = default;
	Error& operator=(Error&&) noexcept = default;

	Response::Status code;
	string_view details;
};

class Exception: public std::runtime_error
{
public:
	explicit Exception(Response::Status code, string_view details = {}):
		runtime_error{ "HTTP error" },
		error{code, details}
	{}

	explicit Exception(string_view details):
		Exception{ Response::Status::internal_server_error, details} {}

	template <int N>
	explicit Exception(const char (&details)[N]):
		runtime_error{details},
		error{ Response::Status::internal_server_error, string_view{details, N} }
	{}

	Response::Status status_code() const noexcept { return error.code; }
	string_view detail_string() const noexcept { return error.details;  }
private:
	const Error error;
};
}