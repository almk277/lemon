#pragma once

#include "logger_imp.hpp"
#include "server.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <vector>
#include <thread>

class parameters;
class options;
class router;

class manager: boost::noncopyable
{
public:
	explicit manager(const parameters &params);
	~manager();

	void run();

private:
	void init();
	void run_worker(unsigned n) noexcept;

	unsigned n_workers = 0;
	boost::asio::io_service service;
	boost::asio::io_service::work work;
	boost::asio::signal_set signal_set;
	std::vector<std::thread> workers;
	const parameters &params;
	common_logger lg;
	std::vector<std::unique_ptr<server>> srv;
};
