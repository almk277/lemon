#include "response_status.hpp"
#include <cstdlib>

struct string_list
{
	const string_view *array;
	int size;
	
	template <int N>
	constexpr string_list(const string_view (&a)[N]):
		array{a},
		size{N}
	{}
};

//TODO precise code instead of 0
constexpr string_view unsupported = "0 unknown HTTP error"_w;

constexpr string_view m0[] = {
	unsupported
};

constexpr string_view m1[] = {
	"100 Continue"_w,
	"101 Switching Protocols"_w,
};

constexpr string_view m2[] = {
	"200 OK"_w,
};

constexpr string_view m3[] = {
	"300 Multiple Choices"_w,
	"301 Moved Permanently"_w,
};

constexpr string_view m4[] = {
	"400 Bad Request"_w,
	"401 Unauthorized"_w,
	"402 Payment Required"_w,
	"403 Forbidden"_w,
	"404 Not Found"_w,
	"405 Method Not Allowed"_w,
};

constexpr string_view m5[] = {
	"500 Internal Server Error"_w,
	"501 Not Implemented"_w,
	"502 Bad Gateway"_w,
	"503 Service Unavailable"_w,
	"504 Gateway Timeout"_w,
	"505 HTTP Version Not Supported"_w,
};

// MSVS may crash on constexpr here
const string_list strings[] = {
	m0, m1, m2, m3, m4, m5
};

const string_view &response_status_string(response_status status) noexcept
{
	auto dv = std::div(static_cast<int>(status), 100);
	
	int group = dv.quot;
	if (BOOST_UNLIKELY(group > 5))
		return unsupported;

	auto sublist = strings[group];
	int subcode = dv.rem;
	if (BOOST_UNLIKELY(subcode >= sublist.size))
		return unsupported;

	return sublist.array[subcode];
}
