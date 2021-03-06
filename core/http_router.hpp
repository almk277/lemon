#pragma once
#include "string_view.hpp"
#include "options.hpp"
#include <boost/core/noncopyable.hpp>
#include <memory>
#include <utility>
#include <vector>

class ModuleManager;

namespace http
{
struct RequestHandler;

class Router: boost::noncopyable
{
public:
	Router(const ModuleManager& manager, const Options::RouteList& routes);

	RequestHandler* resolve(string_view path) const;
	
	struct Matcher
	{
		virtual ~Matcher() = default;

		virtual bool match(string_view s) const = 0;
	};

private:
	std::vector<std::pair<std::unique_ptr<const Matcher>,
		std::shared_ptr<RequestHandler>>> matchers;
};
}