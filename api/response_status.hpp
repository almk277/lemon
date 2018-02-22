#pragma once

#include "utility.hpp"
#include <ostream>

enum class response_status
{
	CONTINUE = 100,
	SWITCHING_PROTOCOLS = 101,

	OK = 200,

	MULTIPLE_CHOICES = 300,
	MOVED_PERMANENTLY = 301,
	FOUND = 302,
	SEE_OTHER = 303,

	BAD_REQUEST = 400,
	UNAUTHORIZED = 401,
	PAYMENT_REQUIRED = 402,
	FORBIDDEN = 403,
	NOT_FOUND = 404,
	METHOD_NOT_ALLOWED = 405,

	INTERNAL_SERVER_ERROR = 500,
	NOT_IMPLEMENTED = 501,
	BAD_GATEWAY = 502,
	SERVICE_UNAVAILABLE = 503,
	GATEWAY_TIMEOUT = 504,
	HTTP_VERSION_NOT_SUPPORTED = 505,
};

string_view response_status_string(response_status status) noexcept;

inline std::ostream &operator<<(std::ostream &strm, response_status status)
{
	return strm << static_cast<int>(status);
}