#include "client.hpp"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
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
template <typename Task, typename Handler>
struct arena_handler
{
	using allocator_type = arena::allocator<Handler>;

	arena_handler(const Task &t, Handler h) noexcept: a{ t.get_arena() }, h{ std::move(h) } {}

	allocator_type get_allocator() const noexcept
	{
		return a.make_allocator<Handler>("asio handler");
	}

	template <typename ...Args>
	void operator()(Args &&...args)
	{
		h(std::forward<Args>(args)...);
	}

private:
	arena &a;
	const Handler h;
};

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

client::client(boost::asio::io_context &context, socket &&sock, std::shared_ptr<const options> opt,
	std::shared_ptr<const router> router, server_logger &lg) noexcept:
	sock{std::move(sock)},
	opt(std::move(opt)),
	rout(std::move(router)),
	lg{lg, this->sock.remote_endpoint().address()},
	builder{start_task_id, *this->opt},
	next_send_id{start_task_id},
	send_barrier{context}
{
	lg.info("connection established"sv);
}

client::~client()
{
	if (sock.is_open()) {
		error_code ec;
		sock.shutdown(socket::shutdown_both, ec);
		if (ec)
			lg.error("socket shutdown failed: ", ec);
	}
	lg.info("connection closed"sv);
}

void client::make(boost::asio::io_context &context, socket &&sock, std::shared_ptr<const options> opt,
	std::shared_ptr<const router> rout, server_logger &lg)
{
	auto c = std::allocate_shared<client>(client_allocator, context, std::move(sock),
		move(opt), move(rout), lg);
	auto it = c->builder.prepare_task(c);
	c->start_recv(it);
}

void client::start_recv(const incomplete_task &it)
{
	const auto b = builder.get_memory(it);
	sock.async_read_some(buffer(b), arena_handler{ it,
		[this, it](const error_code &ec, size_t bytes_transferred)
		{
			on_recv(ec, bytes_transferred, it);
		} });
}

void client::on_recv(const error_code &ec,
                     size_t bytes_transferred,
	                 const incomplete_task &it) noexcept
{
	const auto eof = ec == boost::asio::error::eof;
	it.lg().debug("received bytes: "sv, bytes_transferred, eof ? ", and EOF"sv : ""sv);
	if (BOOST_UNLIKELY(ec && !eof)) {
		it.lg().error("failed to read request: "sv, ec);
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
	post(sock.get_executor(), arena_handler{ rt, [this, rt]
		{
			try {
				auto tr = rt.run();
				start_send(tr);
			} catch (std::exception& e) {
				rt.lg().error("send response error: "sv, e.what());
			} catch (...) {
				rt.lg().error("send response unknown error"sv);
			}
		}
	});
}

void client::start_send(const task::result &tr)
{
	dispatch(send_barrier, arena_handler{ tr, [this, tr]
		{
			if (BOOST_LIKELY(tr.get_id() == next_send_id)) {
				tr.lg().debug("sending task result..."sv);
				async_write(sock, tr, arena_handler{ tr,
					[this, tr](const error_code &ec, size_t) { on_sent(ec, tr); } });
			} else {
				tr.lg().debug("queueing task result"sv);
				BOOST_ASSERT(boost::find(send_q, tr) == send_q.end());
				auto it = boost::upper_bound(send_q, tr);
				send_q.insert(it, tr);
				BOOST_ASSERT(boost::is_sorted(send_q));
			}
		} });
}

void client::on_sent(const error_code &ec, const task::result &tr) noexcept
{
	try {
		if (ec) {
			tr.lg().error("failed to send task result: "sv, ec);
			send_q.clear();  //TODO cancel tasks
		} else {
			tr.lg().debug("task result sent"sv);
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
		tr.lg().error("response queue error: "sv, e.what());
	}
}