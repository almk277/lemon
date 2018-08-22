#pragma once

#include "utility.hpp"
#include <boost/core/noncopyable.hpp>
#include <vector>
#include <memory>
#include <tuple>
struct request_handler;
class options;
class rh_manager;

class router: boost::noncopyable
{
public:
	explicit router(const rh_manager &rhman, const options &opts);
	~router() = default;

	std::shared_ptr<request_handler> resolve(string_view path) const;
	
	struct matcher
	{
		virtual ~matcher() = default;

		virtual bool match(string_view s) const = 0;
	};

private:
	std::vector<std::tuple<std::unique_ptr<const matcher>,
		std::shared_ptr<request_handler>>> matchers;
};
