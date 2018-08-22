#include "task.hpp"
#include "client.hpp"
#include "http_error.hpp"
#include "request_handler.hpp"
#include "router.hpp"
#include "string_builder.hpp"
#include <boost/log/attributes/constant.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/scope_exit.hpp>

static boost::fast_pool_allocator<task,
	boost::default_user_allocator_malloc_free> task_allocator;

task::task(ident id, std::shared_ptr<client> cl) noexcept:
	id{id},
	cl{cl},
	lg{ cl->get_logger() },
	a{lg},
	req{a},
	resp{a},
	resp_buf{a.make_allocator<buffer>("task::resp_buf")},
	rout{cl->get_router()}
{
	lg.add(logger_imp::attr_name.task, boost::log::attributes::make_constant(id));
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
	if (!done) {
		const auto path = req.url.path;
		lg.debug("resolving: ", path);
		auto h = rout->resolve(path);
		try {
			if (BOOST_LIKELY(static_cast<bool>(h))) {
				lg.debug("handler found: ", h->get_name());
				handle_request(*h);
				lg.debug("handler finished");
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
		done = true;
	}

	serialize_resp();
}

void task::handle_request(request_handler &h)
{
	auto module_attr = lg.add(logger_imp::attr_name.module,
		boost::log::attributes::make_constant(h.get_name()));
	BOOST_SCOPE_EXIT(&lg, &module_attr) {
		lg.remove(module_attr);
	} BOOST_SCOPE_EXIT_END

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

static string_view http_version_string(message::http_version_type v)
{
	static constexpr string_view strings[] = {
		"HTTP/1.0 "_w,
		"HTTP/1.1 "_w
	};
	return strings[static_cast<int>(v)];
}

struct task::buffer_builder
{
	explicit buffer_builder(buffer_list &bufs):
		bufs{ bufs }
	{}
	buffer_builder &operator<<(string_view s)
	{
		bufs.emplace_back(s.data(), s.length());
		return *this;
	}

	buffer_list &bufs;
};

//TODO avoid this
void task::serialize_resp()
{
	auto n_chunks = 4 * resp.headers.size() + resp.body.size() + 7;
	resp_buf.reserve(n_chunks);
	buffer_builder builder{resp_buf};

	builder
		<< http_version_string(resp.http_version)
		<< response_status_string(resp.code)
		<< response::NL;
	for (auto &h: resp.headers)
		builder << h.name << response::header::SEP << h.value << response::NL;
	builder << response::NL;
	for (auto &chunk: resp.body)
		builder << chunk;
}
