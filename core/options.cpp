#include "options.hpp"
#include "parameters.hpp"
#include "logger.hpp"
#include <tuple>

options::options(const parameters &p, logger &lg):
	n_workers{2},
	headers_size{4 * 1024},
	log{
		{log_types::console{}, log_types::severity::debug},
		{log_types::console{}}
	},
	servers{
		{
			8080,
			{
				{route::prefix{"/file/"}, "StaticFile"},
				{route::prefix{"/"}, "Testing"},
			}
		}
	}
{
	lg.debug("options initialized");
}

static bool operator==(const options::route::equal& lhs, const options::route::equal& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const options::route::prefix& lhs, const options::route::prefix& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const options::route::regex& lhs, const options::route::regex& rhs)
{
	return lhs.re == rhs.re;
}

static bool operator==(const options::route& lhs, const options::route& rhs)
{
	auto tie = [](const auto& r) { return std::tie(r.matcher, r.handler); };
	return tie(lhs) == tie(rhs);
}

bool operator==(const options::server &lhs, const options::server &rhs)
{
	auto tie = [](const auto &s) { return std::tie(s.listen_port, s.routes); };
	return tie(lhs) == tie(rhs);
}
