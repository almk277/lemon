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

class options;
class router;

class client:
	public std::enable_shared_from_this<client>,
	boost::noncopyable,
	leak_checked<client>
{
public:
	using socket = boost::asio::ip::tcp::socket;

	client(boost::asio::io_context &context, socket &&sock, std::shared_ptr<const options> opt,
		std::shared_ptr<const router> router, server_logger &lg) noexcept;
	~client();

	static void make(boost::asio::io_context &context, socket &&sock, std::shared_ptr<const options> opt,
		std::shared_ptr<const router> rout, server_logger &lg);

	client_logger &get_logger() noexcept { return lg; }
	const router &get_router() const noexcept { return *rout; }

private:
	static constexpr task_ident start_task_id = task::start_id;

	socket sock;
	const std::shared_ptr<const options> opt;
	const std::shared_ptr<const router> rout;
	client_logger lg;
	task_builder builder;
	task_ident next_send_id;
	boost::container::list<task::result> send_q;
	boost::asio::io_context::strand send_barrier;

	void start_recv(const incomplete_task &it);
	void on_recv(const boost::system::error_code &ec,
		         std::size_t bytes_transferred,
		         const incomplete_task &it) noexcept;
	void run(const ready_task &rt) noexcept;
	void start_send(const task::result &tr);
	void on_sent(const boost::system::error_code &ec, const task::result &tr) noexcept;
};