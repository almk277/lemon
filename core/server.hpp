#pragma once
#include "logger_imp.hpp"
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>

struct environment;

class server
{
public:
	server(boost::asio::io_service &service, std::uint16_t port,
		std::shared_ptr<const environment> env);

	void run();

private:
	using tcp = boost::asio::ip::tcp;

	void start_accept();

	tcp::acceptor acceptor;
	tcp::socket sock;
	const std::shared_ptr<const environment> env;
	server_logger lg;
};
