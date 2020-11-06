#include "client.hpp"
#include "algorithm.hpp"
#include "visitor.hpp"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/range/algorithm/upper_bound.hpp>
#include <boost/range/algorithm_ext/is_sorted.hpp>
#include <exception>
#include <utility>

namespace
{
using boost::system::error_code;
using std::size_t;

boost::fast_pool_allocator<Client,
	boost::default_user_allocator_malloc_free,
	boost::details::pool::null_mutex> client_allocator;

//TODO use memory pool instead of arena
template <typename Task, typename Handler>
struct ArenaHandler
{
	using allocator_type = Arena::Allocator<Handler>;

	ArenaHandler(const Task &t, Handler h) noexcept: a{ t.get_arena() }, h{ std::move(h) } {}

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
	Arena &a;
	const Handler h;
};
}

Client::Client(boost::asio::io_context &context, Socket &&sock, std::shared_ptr<const Options> opt,
	std::shared_ptr<const Router> router, ServerLogger &lg) noexcept:
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

Client::~Client()
{
	if (sock.is_open()) {
		error_code ec;
		sock.shutdown(Socket::shutdown_both, ec);
		if (ec)
			lg.error("socket shutdown failed: ", ec);
	}
	lg.info("connection closed"sv);
}

void Client::make(boost::asio::io_context &context, Socket &&sock, std::shared_ptr<const Options> opt,
	std::shared_ptr<const Router> rout, ServerLogger &lg)
{
	auto c = std::allocate_shared<Client>(client_allocator, context, std::move(sock),
		move(opt), move(rout), lg);
	auto it = c->builder.prepare_task(c);
	c->start_recv(it);
}

void Client::start_recv(const IncompleteTask &it)
{
	const auto b = builder.get_memory(it);
	sock.async_read_some(buffer(b), ArenaHandler{ it,
		[this, it](const error_code &ec, size_t bytes_transferred)
		{
			on_recv(ec, bytes_transferred, it);
		} });
}

void Client::on_recv(const error_code &ec,
                     size_t bytes_transferred,
	                 const IncompleteTask &it) noexcept
{
	const auto eof = ec == boost::asio::error::eof;
	it.lg().debug("received bytes: "sv, bytes_transferred, eof ? ", and EOF"sv : ""sv);
	if (BOOST_UNLIKELY(ec && !eof)) {
		it.lg().error("failed to read request: "sv, ec);
		return;
	}

	try {
		for (auto &&t : builder.make_tasks(shared_from_this(), it, bytes_transferred, eof))
			visit(Visitor{
				[this](const IncompleteTask &t) { start_recv(t); },
			    [this](const ReadyTask &t)      { run(t); },
			    [this](const Task::Result &t)    { start_send(t); },
			}, t);
	} catch (std::exception &re) {
		//TODO check if 'it' is actual task
		it.lg().error(re.what());
		start_send(TaskBuilder::make_error_task(it,
			HttpError{Response::Status::internal_server_error, re.what()}));
	}
}

void Client::run(const ReadyTask &rt) noexcept
{
	post(sock.get_executor(), ArenaHandler{ rt, [this, rt]
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

void Client::start_send(const Task::Result &tr)
{
	dispatch(send_barrier, ArenaHandler{ tr, [this, tr]
		{
			if (BOOST_LIKELY(tr.get_id() == next_send_id)) {
				tr.lg().debug("sending task result..."sv);
				async_write(sock, tr, ArenaHandler{ tr,
					[this, tr](const error_code &ec, size_t) { on_sent(ec, tr); } });
			} else {
				tr.lg().debug("queueing task result"sv);
				BOOST_ASSERT(!contains(send_q, tr));
				auto it = boost::upper_bound(send_q, tr);
				send_q.insert(it, tr);
				BOOST_ASSERT(boost::is_sorted(send_q));
			}
		} });
}

void Client::on_sent(const error_code &ec, const Task::Result &tr) noexcept
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