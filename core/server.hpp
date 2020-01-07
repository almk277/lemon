#pragma once
#include "logger_imp.hpp"
#include "options.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>

class router;
class rh_manager;

class server: boost::noncopyable
{
public:
	server(boost::asio::io_service &service, std::shared_ptr<const options> global_opt,
		const options::server &server_opt, const rh_manager &rhman);
	~server();

	const options::server &get_options() const { return server_opt; }

	server_logger lg;

private:
	using tcp = boost::asio::ip::tcp;

	void start_accept();

	boost::asio::io_service &service;
	tcp::acceptor acceptor;
	tcp::socket sock;
	const std::shared_ptr<const options> global_opt;
	const options::server &server_opt;
	const std::shared_ptr<const router> rout;
};
