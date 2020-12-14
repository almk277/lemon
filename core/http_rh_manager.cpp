#include "http_rh_manager.hpp"
#include "http_request_handler.hpp"

namespace http
{
void RhManager::add(std::shared_ptr<RequestHandler> h)
{
	handlers.emplace(h->get_name(), h);
}

std::shared_ptr<RequestHandler> RhManager::operator[](string_view name) const
{
	auto found = handlers.find(name);
	return found == handlers.end() ? nullptr : found->second;
}
}