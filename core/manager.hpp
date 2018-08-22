#pragma once

#include "options.hpp"
#include "logger_imp.hpp"
#include "rh_manager.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <memory>
#include <vector>
#include <thread>

class router;

class manager: boost::noncopyable
{
public:
	explicit manager(options &&opt);
	~manager();

	void run();
	
	const options &get_options() const noexcept { return opt; }
	std::shared_ptr<const router> get_router() const noexcept { return rout; }

private:
	using tcp = boost::asio::ip::tcp;

	const unsigned n_workers;
	boost::asio::io_service service;
	boost::asio::io_service::work work;
	tcp::acceptor acceptor;
	tcp::socket sock;
	boost::asio::signal_set signal_set;
	std::vector<std::thread> workers;
	options opt;
	logger_imp lg;
	rh_manager rhman;
	std::shared_ptr<router> rout;

	void start_accept();
	void add_worker();
};
