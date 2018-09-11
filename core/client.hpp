#pragma once

#include "task.hpp"
#include "task_builder.hpp"
#include "logger_imp.hpp"
#include "leak_checked.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <boost/container/list.hpp>
#include <cstddef>
#include <memory>

class manager;
class options;
class router;

class client:
	public std::enable_shared_from_this<client>,
	boost::noncopyable,
	leak_checked<client>
{
public:
	using socket = boost::asio::ip::tcp::socket;

	client(manager &man, socket &&sock) noexcept;
	~client();

	static void make(manager &man, socket &&sock);

	const options &get_options() const noexcept { return opt; }
	logger_imp &get_logger() noexcept { return lg; }
	const logger_imp &get_logger() const noexcept { return lg; }
	std::shared_ptr<const router> get_router() const noexcept { return rout; }

private:
	static constexpr task::ident start_task_id = task::start_id;

	socket sock;
	const options &opt;
	logger_imp lg;
	task_builder builder;
	task::ident next_send_id;
	boost::container::list<task_result> send_q;
	boost::asio::io_service::strand send_barrier;
	const std::shared_ptr<const router> rout;

	void start_recv(incomplete_task it);
	void on_recv(const boost::system::error_code &ec,
		         std::size_t bytes_transferred,
		         incomplete_task it) noexcept;
	void run(ready_task t) noexcept;
	void start_send(task_result tr);
	void on_sent(const boost::system::error_code &ec, task_result tr) noexcept;
};