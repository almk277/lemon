#include "client.hpp"
#include "manager.hpp"
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/bind/bind.hpp>
#include <exception>
#include <algorithm>

using boost::system::error_code;
using std::size_t;

static boost::fast_pool_allocator<task,
	boost::default_user_allocator_malloc_free,
	boost::details::pool::null_mutex> client_allocator;

template <typename Handler>
struct arena_handler
{
	arena_handler(arena &a, Handler h): a{ a }, h { h } {}

	template <typename ...Args>
	void operator()(Args &&...args)
	{
		h(std::forward<Args>(args)...);
	}

	friend void *asio_handler_allocate(std::size_t size,
		arena_handler<Handler> *handler)
	{
		return handler->a.alloc(size, "asio handler");
	}

	friend void asio_handler_deallocate(void *p, std::size_t size,
		arena_handler<Handler> *handler)
	{
		handler->a.free(p, size, "asio handler");
	}

	arena &a;
	Handler h;
};

template <typename Handler>
arena_handler<Handler> make_arena_handler(arena &a, Handler h)
{
	return{ a, h };
}

client::client(manager &man, socket &&s) noexcept:
	sock{std::move(s)},
	opt{man.get_options()},
	lg{},
	builder{start_task_id, opt, lg},
	next_send_id{start_task_id},
	send_barrier{sock.get_io_service()},
	rout{man.get_router()}
{
	lg.add(logger_imp::attr_name.address, boost::log::attributes::make_constant(
		sock.remote_endpoint().address()));

	lg.info("connection established"_w);

}

client::~client()
{
	if (sock.is_open())
		sock.shutdown(socket::shutdown_both); //TODO noexcept version
	lg.info("connection closed"_w);
}

void client::make(manager &man, socket &&sock)
{
	auto c = std::allocate_shared<client>(client_allocator, man, std::move(sock));
	auto it = c->builder.prepare_task(c);
	c->start_recv(it);
}

void client::start_recv(incomplete_task it)
{
	using namespace boost::asio::placeholders;

	const auto b = builder.get_memory(it);
	sock.async_read_some(buffer(b), make_arena_handler(it.get_arena(),
		boost::bind(&client::on_recv, this, error, bytes_transferred, it)));
}

void client::on_recv(const error_code &ec,
                     size_t bytes_transferred,
	                 incomplete_task it) noexcept
{
	const bool eof = ec == boost::asio::error::eof;
	lg.debug("received ", bytes_transferred, " bytes, EOF=", eof);
	if (BOOST_UNLIKELY(ec && !eof)) {
		lg.error("failed to read request:"_w, ec);
		return;
	}

	try {
		auto shared_this = shared_from_this();
		builder.feed(bytes_transferred, eof);
		auto res = builder.try_make_task(it, shared_this);
		const auto first_task = res.t;
		while (res.s == task_builder::status::TRY_MAKE_AGAIN)
		{
			res = builder.try_make_task(it, shared_this);
			if (res.t)
				sock.get_io_service().post(make_arena_handler(it.get_arena(),
					boost::bind(&client::run, this, *res.t)));
		}

		if (res.s == task_builder::status::FEED_AGAIN)
			start_recv(it);

		if (first_task)
			run(*first_task);

	} catch (std::exception &re) {
		lg.error(re.what());
		run(builder.make_error_task(it));
	}
}

void client::run(ready_task t) noexcept
{
	try {
		auto ft = t.run();
		send_barrier.dispatch(make_arena_handler(t.get_arena(),
			boost::bind(&client::start_send, this, ft)));
	} catch (std::exception &e) {
		lg.error("send response error: "_w, e.what());
	} catch (...) {
		lg.error("send response unknown error"_w);
	}
}

void client::start_send(task_result tr)
{
	using namespace boost::asio::placeholders;

	if (BOOST_LIKELY(tr.get_id() == next_send_id)) {
		lg.debug("sending task #", tr.get_id(), " result...");
		async_write(sock, tr, make_arena_handler(tr.get_arena(),
			boost::bind(&client::on_sent, this, error, tr)));
	} else {
		lg.debug("queueing task #", tr.get_id(), " result");
		BOOST_ASSERT(std::find(send_q.begin(), send_q.end(), tr) == send_q.end());
		auto it = std::upper_bound(send_q.begin(), send_q.end(), tr);
		send_q.insert(it, std::move(tr));
		BOOST_ASSERT(std::is_sorted(send_q.begin(), send_q.end()));
	}
}

void client::on_sent(const error_code &ec, task_result tr) noexcept
{
	try {
		auto id = tr.get_id();
		if (ec) {
			lg.error("failed to send task #"_w, id, " result: ", ec);
			send_q.clear();  //TODO cancel tasks
		}
		else {
			lg.debug("task #", id, " result sent");
			++next_send_id;
			if (BOOST_UNLIKELY(!send_q.empty())
				&& send_q.front().get_id() == next_send_id) {
				lg.debug("dequeueing task #", next_send_id);
				start_send(send_q.front());
				send_q.pop_front();
			}
		}
	} catch (std::exception &e) {
		lg.error("response queue error: "_w, e.what());
	}
}