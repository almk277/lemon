#pragma once
#include "task_ident.hpp"
#include "task_builder.hpp"
#include "logger_imp.hpp"
#include "leak_checked.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <boost/container/list.hpp>
#include <cstddef>
#include <memory>

class Options;
class Router;

class Client:
	public std::enable_shared_from_this<Client>,
	boost::noncopyable,
	LeakChecked<Client>
{
public:
	using Socket = boost::asio::ip::tcp::socket;

	Client(boost::asio::io_context &context, Socket &&sock, std::shared_ptr<const Options> opt,
		std::shared_ptr<const Router> router, ServerLogger &lg) noexcept;
	~Client();

	static void make(boost::asio::io_context &context, Socket &&sock, std::shared_ptr<const Options> opt,
		std::shared_ptr<const Router> rout, ServerLogger &lg);

	ClientLogger &get_logger() noexcept { return lg; }
	const Router &get_router() const noexcept { return *rout; }

private:
	static constexpr TaskIdent start_task_id = Task::start_id;

	Socket sock;
	const std::shared_ptr<const Options> opt;
	const std::shared_ptr<const Router> rout;
	ClientLogger lg;
	TaskBuilder builder;
	TaskIdent next_send_id;
	boost::container::list<Task::Result> send_q;
	boost::asio::io_context::strand send_barrier;

	void start_recv(const IncompleteTask &it);
	void on_recv(const boost::system::error_code &ec,
		         std::size_t bytes_transferred,
		         const IncompleteTask &it) noexcept;
	void run(const ReadyTask &rt) noexcept;
	void start_send(const Task::Result &tr);
	void on_sent(const boost::system::error_code &ec, const Task::Result &tr) noexcept;
};