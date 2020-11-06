#include "request_handler.hpp"
#include "http_error.hpp"

void RequestHandler::get(Request&, Response&, Context&)
{
	throw HttpException(Response::Status::method_not_allowed, "GET"sv);
}

void RequestHandler::head(Request&, Response&, Context&)
{
	throw HttpException(Response::Status::method_not_allowed, "HEAD"sv);
}

void RequestHandler::post(Request&, Response&, Context&)
{
	throw HttpException(Response::Status::method_not_allowed, "POST"sv);
}

void RequestHandler::method(string_view method_name,
	Request&, Response&, Context&)
{
	throw HttpException(Response::Status::method_not_allowed, method_name);
}