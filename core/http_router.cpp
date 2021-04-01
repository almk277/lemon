#include "http_router.hpp"
#include "http_request_handler.hpp"
#include "module_manager.hpp"
#include <algorithm>
#include <regex>
#include <string>

namespace http
{
namespace
{
class ExactMatcher: public Router::Matcher
{
public:
	explicit ExactMatcher(std::string patt) : patt{ move(patt) } {}
	bool match(string_view s) const noexcept override
	{
		return s == patt;
	}
private:
	const std::string patt;
};

class PrefixMatcher: public Router::Matcher
{
public:
	explicit PrefixMatcher(std::string prefix) : prefix{ move(prefix) } {}
	bool match(string_view s) const noexcept override
	{
		return prefix.length() <= s.length()
			&& equal(prefix.begin(), prefix.end(), s.begin());
	}
private:
	const std::string prefix;
};

class RegexMatcher: public Router::Matcher
{
public:
	explicit RegexMatcher(const std::string& s) : re{ s, std::regex_constants::optimize } {}
	bool match(string_view s) const override
	{
		return regex_match(s.begin(), s.end(), re);
	}
private:
	const std::regex re;
};

struct MatchBuilder
{
	using result_type = std::unique_ptr<Router::Matcher>;

	auto operator()(const Options::Route::Equal& r) const -> result_type
	{
		return std::make_unique<ExactMatcher>(r.str);
	}
	auto operator()(const Options::Route::Prefix& r) const -> result_type
	{
		return std::make_unique<PrefixMatcher>(r.str);
	}
	auto operator()(const Options::Route::Regex& r) const -> result_type
	{
		return std::make_unique<RegexMatcher>(r.re);
	}
};
}

Router::Router(const ModuleManager& manager, const Options::RouteList& routes)
{
	matchers.reserve(routes.size());
	for (auto& r: routes) {
		auto rh = manager.get_handler(r.handler);
		if (!rh)
			throw Options::Error{ r.handler + ": request handler not found" };
		auto http_rh = std::dynamic_pointer_cast<RequestHandler>(rh);
		if (!http_rh)
			throw Options::Error{ r.handler + ": not HTTP request handler" };
		matchers.emplace_back(
			visit(MatchBuilder{}, r.matcher),
			move(http_rh));
	}
}

RequestHandler* Router::resolve(string_view path) const
{
	for (auto& [matcher, handler]: matchers)
		if (matcher->match(path))
			return handler.get();

	return nullptr;
}
}
