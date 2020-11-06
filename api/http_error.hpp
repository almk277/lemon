#pragma once
#include "string_view.hpp"
#include "message.hpp"
#include <stdexcept>

struct HttpError
{
	explicit HttpError(Response::Status code,
		string_view details = {}) noexcept:
		code{code},
		details{details}
	{}
	HttpError(const HttpError&) noexcept = default;
	HttpError(HttpError&&) noexcept = default;
	~HttpError() = default;
	HttpError &operator=(const HttpError&) noexcept = default;
	HttpError &operator=(HttpError&&) noexcept = default;

	Response::Status code;
	string_view details;
};

class HttpException: public std::runtime_error
{
public:
	explicit HttpException(Response::Status code, string_view details = {}):
		runtime_error{ "HTTP error" },
		error{code, details}
	{}

	explicit HttpException(string_view details):
		HttpException{ Response::Status::internal_server_error, details} {}

	template <int N>
	explicit HttpException(const char (&details)[N]):
		runtime_error{details},
		error{ Response::Status::internal_server_error, string_view{details, N} }
	{}

	Response::Status status_code() const noexcept { return error.code; }
	string_view detail_string() const noexcept { return error.details;  }
private:
	const HttpError error;
};