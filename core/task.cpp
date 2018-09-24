#include "task.hpp"
#include "client.hpp"
#include "http_error.hpp"
#include "request_handler.hpp"
#include "router.hpp"
#include "string_builder.hpp"
#include <boost/pool/pool_alloc.hpp>
#include <boost/concept_check.hpp>

static boost::fast_pool_allocator<task,
	boost::default_user_allocator_malloc_free> task_allocator;

task::task(ident id, std::shared_ptr<client> cl) noexcept:
	id{id},
	cl{cl},
	lg{ cl->get_logger(), id},
	a{lg},
	req{a},
	resp{a},
	rout{cl->get_router()}
{
	lg.debug("task created");
}

task::~task()
{
	lg.debug("task removed");
}

std::shared_ptr<task> task::make(ident id, std::shared_ptr<client> cl)
{
	return std::allocate_shared<task>(task_allocator, id, cl);
}

void task::run()
{
	const auto path = req.url.path;
	lg.debug("resolving: ", path);
	auto h = rout->resolve(path);
	try {
		if (BOOST_LIKELY(static_cast<bool>(h))) {
			lg.debug("handler found: ", h->get_name());
			handle_request(*h);
			lg.debug("handler finished: ", h->get_name());
		}
		else {
			lg.debug("HTTP error 404");
			make_error(response_status::NOT_FOUND);
		}
	}
	catch (http_exception &he) {
		lg.debug("HTTP error ", he.status_code(), " ", he.detail_string());
		make_error(he.status_code());
	}
	catch (std::exception &e) {
		lg.error("internal error in module: ", h->get_name(), ": ", e.what());
		make_error(response_status::INTERNAL_SERVER_ERROR);
	}
	catch (...) {
		lg.error("unknown exception in module: ", h->get_name());
		make_error(response_status::INTERNAL_SERVER_ERROR);
	}
}

void task::handle_request(request_handler &h)
{
	module_logger_guard mlg{ lg, h.get_name() };
	request_handler::context ctx{ a, lg };
	
	switch (req.method.type) {
	using method = request::method_s::type_e;
	case method::GET: h.get(req, resp, ctx); break;
	case method::HEAD: h.head(req, resp, ctx); break;
	case method::POST: h.post(req, resp, ctx); break;
	case method::OTHER:
		h.method(req.method.name, req, resp, ctx);
		break;
	default: BOOST_ASSERT(0);
	}
}

void task::make_error(response_status code) noexcept
{
	//TODO cache buffers
	resp.http_version = req.http_version;
	resp.code = code;
	resp.body.clear();
	resp.body.emplace_back(response_status_string(code));

	resp.headers.clear();
	resp.headers.emplace_back("Content-Type"_w, "text/plain"_w);

	auto clen_str = string_builder{ a }.convert(resp.body.front().length());
	resp.headers.emplace_back("Content-Length"_w, clen_str);
}

BOOST_CONCEPT_ASSERT((boost::BidirectionalIterator<task_result::const_iterator>));