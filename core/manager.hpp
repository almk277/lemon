#pragma once

#include "logger_imp.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <vector>
#include <map>
#include <thread>

class options;
class parameters;
class server;

class manager: boost::noncopyable
{
public:
	explicit manager(const parameters &params);
	~manager();

	void run();
	//TODO find a way to call it
	void reinit();

private:
	void init();
	void init_servers(const std::shared_ptr<const options> &opts);
	void init_workers(const std::shared_ptr<const options> &opts);
	void add_worker();
	void remove_worker();
	void run_worker() noexcept;
	void finalize_worker(std::thread::id id);

	boost::asio::io_context master_ctx;
	boost::asio::io_context worker_ctx;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> master_work;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> worker_work;
	boost::asio::signal_set quit_signals;
	std::map<std::thread::id, std::thread> workers;
	unsigned n_workers = 0;
	common_logger lg;
	std::vector<std::unique_ptr<server>> srv;
};
