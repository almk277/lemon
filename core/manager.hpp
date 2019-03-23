#pragma once

#include "logger_imp.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
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
	void reinit();

private:
	void init();
	void init_servers(const std::shared_ptr<const options> &opts);
	void init_workers(const std::shared_ptr<const options> &opts);
	void add_worker();
	void remove_worker();
	void run_worker() noexcept;
	void finalize_worker(std::thread::id id);

	boost::asio::io_service master_srv;
	boost::asio::io_service worker_srv;
	boost::asio::io_service::work master_work;
	boost::asio::io_service::work worker_work;
	boost::asio::signal_set quit_signals;
	std::map<std::thread::id, std::thread> workers;
	unsigned n_workers = 0;
	const parameters &params;
	common_logger lg;
	std::vector<std::unique_ptr<server>> srv;
};
