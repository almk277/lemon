#include "http_task.hpp"
#include "http_error.hpp"
#include "http_parser_.hpp"
#include "http_request_handler.hpp"
#include "http_router.hpp"
#include "string_builder.hpp"
#include "tcp_client.hpp"
#include <boost/pool/pool_alloc.hpp>
#include <boost/concept_check.hpp>

namespace http
{
namespace
{
BOOST_CONCEPT_ASSERT((boost::BidirectionalIterator<Task::Result::const_iterator>));

boost::fast_pool_allocator<Task, boost::default_user_allocator_malloc_free> task_allocator;
}

Task::Task(Ident id, std::shared_ptr<tcp::Client> cl) noexcept:
	id{id},
	cl{cl},
	lg{cl->get_logger(), id},
	a{lg},
	req{a},
	resp{a},
	router{cl->get_router()}
{
	lg.debug("task created");
}

Task::~Task()
{
	lg.debug("task removed");
}

std::shared_ptr<Task> Task::make(Ident id, std::shared_ptr<tcp::Client> cl)
{
	return std::allocate_shared<Task>(task_allocator, id, cl);
}

void Task::run()
{
	lg.access(req.method.name, " ", req.url.all);

	const auto path = req.url.path;
	lg.debug("resolving: ", path);
	auto h = router.resolve(path);
	try {
		if (BOOST_LIKELY(h != nullptr)) {
			lg.debug("handler found: ", h->get_name());
			handle_request(*h);
			lg.debug("handler finished: ", h->get_name());
		} else {
			lg.debug("HTTP error 404");
			make_error(Response::Status::not_found);
		}
	} catch (Exception &he) {
		lg.debug("HTTP error ", he.status_code(), " ", he.detail_string());
		make_error(he.status_code());
	} catch (std::exception &e) {
		lg.error("internal error in module: ", h->get_name(), ": ", e.what());
		make_error(Response::Status::internal_server_error);
	} catch (...) {
		lg.error("unknown exception in module: ", h->get_name());
		make_error(Response::Status::internal_server_error);
	}
}

void Task::handle_request(RequestHandler& h)
{
	ModuleLoggerGuard mlg{ lg, h.get_name() };
	Parser::finalize(req);
	RequestHandler::Context ctx{ a, lg };
	
	switch (req.method.type) {
	using method = Request::Method::Type;
	case method::get: h.get(req, resp, ctx); break;
	case method::head: h.head(req, resp, ctx); break;
	case method::post: h.post(req, resp, ctx); break;
	case method::other:
		h.method(req.method.name, req, resp, ctx);
		break;
	default: BOOST_ASSERT(0);
	}
}

void Task::make_error(Response::Status code) noexcept
{
	//TODO cache buffers
	resp.http_version = req.http_version;
	resp.code = code;
	resp.body.clear();
	resp.body.emplace_back(to_string(code));

	resp.headers.clear();
	resp.headers.emplace_back("Content-Type"sv, "text/plain"sv);

	auto clen_str = StringBuilder{ a }.convert(resp.body.front().length());
	resp.headers.emplace_back("Content-Length"sv, clen_str);
}
}
