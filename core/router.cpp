#include "router.hpp"
#include "options.hpp"
#include "rh_manager.hpp"
#include <boost/variant/static_visitor.hpp>
#include <string>
#include <regex>

class exact_matcher: public router::matcher
{
public:
	explicit exact_matcher(const std::string &patt): patt{patt} {}
	bool match(string_view s) const noexcept override
	{
		return s == patt;
	}
private:
	const std::string patt;
};

class prefix_matcher: public router::matcher
{
public:
	explicit prefix_matcher(const std::string &prefix): prefix{prefix} {}
	bool match(string_view s) const noexcept override
	{
		return s.starts_with(prefix);
	}
private:
	const std::string prefix;
};

class regex_matcher: public router::matcher
{
public:
	explicit regex_matcher(const std::string &s): re{s} {}
	bool match(string_view s) const override
	{
		return regex_match(s.begin(), s.end(), re);
	}
private:
	const std::regex re;
};

struct match_builder: boost::static_visitor<std::unique_ptr<router::matcher>>
{
	auto operator()(const options::route::equal &r) const
	{
		return std::make_unique<exact_matcher>(r.str);
	}
	auto operator()(const options::route::prefix &r) const
	{
		return std::make_unique<prefix_matcher>(r.str);
	}
	auto operator()(const options::route::regex &r) const
	{
		return std::make_unique<regex_matcher>(r.re);
	}
};

router::router(const rh_manager &rhman, const options &opts)
{
	matchers.reserve(opts.routes.size());
	for (auto &r: opts.routes) {
		auto rh = rhman[r.handler];
		if (!rh)
			throw options::error(r.handler + ": module not found");
		matchers.emplace_back(
			boost::apply_visitor(match_builder{}, r.matcher),
			move(rh));
	}
}

std::shared_ptr<request_handler> router::resolve(string_view path) const
{
	for (auto &el: matchers) {
		if (std::get<0>(el)->match(path))
			return std::get<1>(el);
	}
	return nullptr;
}
