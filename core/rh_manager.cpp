#include "rh_manager.hpp"
#include "request_handler.hpp"

void rh_manager::add(std::shared_ptr<request_handler> h)
{
	handlers.emplace(h->get_name(), h);
}

std::shared_ptr<request_handler> rh_manager::operator[](string_view name) const
{
	auto found = handlers.find(name);
	return found == handlers.end() ? nullptr : found->second;
}
