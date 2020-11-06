#pragma once

#include "string_view.hpp"
#include <boost/core/noncopyable.hpp>
#include <memory>
#include <map>
struct RequestHandler;

class RhManager: boost::noncopyable
{
public:
	RhManager() = default;

	void add(std::shared_ptr<RequestHandler> h);
	std::shared_ptr<RequestHandler> operator[](string_view name) const;

private:
	std::map<string_view, std::shared_ptr<RequestHandler>> handlers;
};