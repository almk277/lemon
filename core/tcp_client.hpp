#pragma once
#include "http_task_builder.hpp"
#include "leak_checked.hpp"
#include "logger_imp.hpp"
#include "task_ident.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/container/list.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <memory>

class Options;

namespace http
{
class Router;
}

namespace tcp
{
class Client:
	public std::enable_shared_from_this<Client>,
	boost::noncopyable,
	LeakChecked<Client>
{
public:
	using Socket = boost::asio::ip::tcp::socket;

	Client(boost::asio::io_context& context, Socket&& sock, std::shared_ptr<const Options> opt,
		std::shared_ptr<const http::Router> router, ServerLogger& lg) noexcept;
	~Client();

	static void make(boost::asio::io_context& context, Socket&& sock, std::shared_ptr<const Options> opt,
		std::shared_ptr<const http::Router> rout, ServerLogger& lg);

	ClientLogger& get_logger() noexcept { return lg; }
	const http::Router& get_router() const noexcept { return *router; }

private:
	static constexpr TaskIdent start_task_id = http::Task::start_id;

	Socket sock;
	const std::shared_ptr<const Options> opt;
	const std::shared_ptr<const http::Router> router;
	ClientLogger lg;
	http::TaskBuilder builder;
	TaskIdent next_send_id;
	boost::container::list<http::Task::Result> send_q;
	boost::asio::io_context::strand send_barrier;

	void start_recv(const http::IncompleteTask& it);
	void on_recv(const boost::system::error_code& ec,
		         std::size_t bytes_transferred,
		         const http::IncompleteTask& it) noexcept;
	void run(const http::ReadyTask& rt) noexcept;
	void start_send(const http::Task::Result& tr);
	void on_sent(const boost::system::error_code& ec, const http::Task::Result& tr) noexcept;
};
}