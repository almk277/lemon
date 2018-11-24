#include "client.hpp"
#include "manager.hpp"
#include <boost/asio/write.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/upper_bound.hpp>
#include <boost/range/algorithm_ext/is_sorted.hpp>
#include <exception>
#include <utility>

using boost::system::error_code;
using std::size_t;

static boost::fast_pool_allocator<client,
	boost::default_user_allocator_malloc_free,
	boost::details::pool::null_mutex> client_allocator;

//TODO use memory pool instead of arena
template <typename Handler>
struct arena_handler
{
	arena_handler(arena &a, Handler h) noexcept: a{ a }, h {std::move(h)} {}

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
		arena_handler<Handler> *handler) noexcept
	{
		handler->a.free(p, size, "asio handler");
	}

	arena &a;
	const Handler h;
};

template <typename Handler>
arena_handler<Handler> make_arena_handler(arena &a, Handler h) noexcept
{
	return{ a, h };
}

struct client::task_visitor : boost::static_visitor<>
{
	explicit task_visitor(client &cl) noexcept: cl{ cl } {}

	auto operator()(const incomplete_task &it) const
	{
		cl.start_recv(it);
	}
	auto operator()(const ready_task &rt) const
	{
		cl.run(rt);
	}
	auto operator()(const task::result &tr) const
	{
		cl.start_send(tr);
	}
private:
	client &cl;
};

client::client(socket &&sock, std::shared_ptr<const environment> env, server_logger &lg) noexcept:
	sock{std::move(sock)},
	env{move(env)},
	lg{lg, this->sock.remote_endpoint().address()},
	builder{start_task_id, *this->env->opt},
	next_send_id{start_task_id},
	send_barrier{sock.get_io_service()}
{
	lg.info("connection established"_w);
}

client::~client()
{
	if (sock.is_open()) {
		error_code ec;
		sock.shutdown(socket::shutdown_both, ec);
		if (ec)
			lg.error("socket shutdown failed: ", ec);
	}
	lg.info("connection closed"_w);
}

void client::make(socket &&sock, std::shared_ptr<const environment> env, server_logger &lg)
{
	auto c = std::allocate_shared<client>(client_allocator, std::move(sock), move(env), lg);
	auto it = c->builder.prepare_task(c);
	c->start_recv(it);
}

void client::start_recv(const incomplete_task &it)
{
	const auto b = builder.get_memory(it);
	sock.async_read_some(buffer(b), make_arena_handler(it.get_arena(),
		[this, it](const error_code &ec, size_t bytes_transferred)
		{
			on_recv(ec, bytes_transferred, it);
		}));
}

void client::on_recv(const error_code &ec,
                     size_t bytes_transferred,
	                 const incomplete_task &it) noexcept
{
	const auto eof = ec == boost::asio::error::eof;
	it.lg().debug("received bytes: "_w, bytes_transferred, eof ? ", and EOF"_w : ""_w);
	if (BOOST_UNLIKELY(ec && !eof)) {
		it.lg().error("failed to read request: "_w, ec);
		return;
	}

	try {
		task_visitor v{ *this };
		for (auto &&t : builder.make_tasks(shared_from_this(), it, bytes_transferred, eof))
			apply_visitor(v, t);
	} catch (std::exception &re) {
		//TODO check if 'it' is actual task
		it.lg().error(re.what());
		start_send(task_builder::make_error_task(it, response::status::INTERNAL_SERVER_ERROR));
	}
}

void client::run(const ready_task &rt) noexcept
{
	sock.get_io_service().post(make_arena_handler(rt.get_arena(), [this, rt]
	{
		try {
			auto tr = rt.run();
			start_send(tr);
		} catch (std::exception &e) {
			rt.lg().error("send response error: "_w, e.what());
		} catch (...) {
			rt.lg().error("send response unknown error"_w);
		}
	}));
}

void client::start_send(const task::result &tr)
{
	send_barrier.dispatch(make_arena_handler(tr.get_arena(), [this, tr]
	{
		if (BOOST_LIKELY(tr.get_id() == next_send_id)) {
			tr.lg().debug("sending task result..."_w);
			async_write(sock, tr, make_arena_handler(tr.get_arena(),
				[this, tr](const error_code &ec, size_t)
			{
				on_sent(ec, tr);
			}));
		} else {
			tr.lg().debug("queueing task result"_w);
			BOOST_ASSERT(boost::find(send_q, tr) == send_q.end());
			auto it = boost::upper_bound(send_q, tr);
			send_q.insert(it, tr);
			BOOST_ASSERT(boost::is_sorted(send_q));
		}
	}));
}

void client::on_sent(const error_code &ec, const task::result &tr) noexcept
{
	try {
		if (ec) {
			tr.lg().error("failed to send task result: "_w, ec);
			send_q.clear();  //TODO cancel tasks
		} else {
			tr.lg().debug("task result sent"_w);
			//TODO check if tr was error task
			++next_send_id;
			if (BOOST_UNLIKELY(!send_q.empty())
					&& send_q.front().get_id() == next_send_id) {
				tr.lg().debug("dequeuing task", next_send_id);
				start_send(send_q.front());
				send_q.pop_front();
			}
		}
	} catch (std::exception &e) {
		tr.lg().error("response queue error: "_w, e.what());
	}
}