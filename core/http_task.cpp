#include "http_task.hpp"
#include "http_error.hpp"
#include "http_request_handler.hpp"
#include "http_router.hpp"
#include "string_builder.hpp"
#include "tcp_session.hpp"
#include <boost/pool/pool_alloc.hpp>
#include <boost/concept_check.hpp>
#include <ostream>

namespace http
{
namespace
{
BOOST_CONCEPT_ASSERT((boost::BidirectionalIterator<Task::Result::const_iterator>));

boost::fast_pool_allocator<Task, boost::default_user_allocator_malloc_free> task_allocator;
}

static auto operator<<(std::ostream& stream, const RequestHandler& handler) -> std::ostream&
{
	return stream << handler.get_name();
}

Task::Task(Ident id, std::shared_ptr<tcp::Session> session) noexcept:
	id{id},
	session{session},
	lg{session->get_logger(), id},
	a{lg},
	req{a},
	resp{a},
	router{session->get_router()}
{
	lg.debug("task created");
}

Task::~Task()
{
	lg.debug("task removed");
}

auto Task::resolve() noexcept -> bool
{
	const auto path = req.url.path;
	BOOST_ASSERT(!path.empty());
	
	lg.debug("resolving: ", path);
	handler = router.resolve(path);
	if (handler) {
		lg.debug("handler found: ", *handler);
	} else {
		lg.debug("handler not found; switch to drop mode");
		drop_mode = true;
	}
	
	return handler != nullptr;
}

auto Task::make(Ident id, std::shared_ptr<tcp::Session> session) -> std::shared_ptr<Task>
{
	return std::allocate_shared<Task>(task_allocator, id, move(session));
}

auto Task::run() -> void
{
	lg.access(req.method.name, " ", req.url.all);

	try {
		if (BOOST_LIKELY(handler != nullptr)) {
			lg.debug("start handler");
			handle_request();
			lg.debug("handler finished: ", *handler);
		} else {
			lg.debug("HTTP error 404");
			make_error(Response::Status::not_found);
		}
	} catch (Exception &he) {
		lg.debug("HTTP error ", he.status_code(), " ", he.detail_string());
		make_error(he.status_code());
	} catch (std::exception &e) {
		lg.error("internal error in module: ", *handler, ": ", e.what());
		make_error(Response::Status::internal_server_error);
	} catch (...) {
		lg.error("unknown exception in module: ", *handler);
		make_error(Response::Status::internal_server_error);
	}
}

auto Task::handle_request() -> void
{
	BOOST_ASSERT(handler);
	
	ModuleLoggerGuard mlg{ lg, handler->get_name() };
	RequestHandler::Context ctx{ a, lg };
	
	switch (req.method.type) {
	using method = Request::Method::Type;
	case method::get: handler->get(req, resp, ctx); break;
	case method::head: handler->head(req, resp, ctx); break;
	case method::post: handler->post(req, resp, ctx); break;
	case method::other:
		handler->method(req.method.name, req, resp, ctx);
		break;
	default: BOOST_ASSERT(0);
	}
}

auto Task::make_error(Response::Status code) noexcept -> void
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
