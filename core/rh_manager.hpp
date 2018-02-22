#pragma once

#include "utility.hpp"
#include <memory>
#include <map>
struct request_handler;

class rh_manager: noncopyable
{
public:
	rh_manager();
	~rh_manager() = default;

	std::shared_ptr<request_handler> operator[](string_view name) const;
private:
	void add(std::shared_ptr<request_handler> h);

	std::map<string_view, std::shared_ptr<request_handler>> handlers;
};