#include "request_handler.hpp"
#include "http_error.hpp"

void request_handler::get(request&, response&, context&)
{
	throw http_exception(response::status::METHOD_NOT_ALLOWED, "GET"_w);
}

void request_handler::head(request&, response&, context&)
{
	throw http_exception(response::status::METHOD_NOT_ALLOWED, "HEAD"_w);
}

void request_handler::post(request&, response&, context&)
{
	throw http_exception(response::status::METHOD_NOT_ALLOWED, "POST"_w);
}

void request_handler::method(boost::string_view method_name,
	request&, response&, context&)
{
	throw http_exception(response::status::METHOD_NOT_ALLOWED, method_name);
}
