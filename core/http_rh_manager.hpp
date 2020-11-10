#pragma once
#include "string_view.hpp"
#include <boost/core/noncopyable.hpp>
#include <map>
#include <memory>

namespace http
{
struct RequestHandler;

class RhManager: boost::noncopyable
{
public:
	RhManager() = default;

	void add(std::shared_ptr<http::RequestHandler> h);
	std::shared_ptr<http::RequestHandler> operator[](string_view name) const;

private:
	std::map<string_view, std::shared_ptr<http::RequestHandler>> handlers;
};
}