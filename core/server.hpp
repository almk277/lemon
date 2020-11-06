#pragma once
#include "logger_imp.hpp"
#include "options.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>

class Router;
class RhManager;

class Server: boost::noncopyable
{
public:
	Server(boost::asio::io_context &context, std::shared_ptr<const Options> global_opt,
		const Options::Server &server_opt, const RhManager &rhman);
	~Server();

	const Options::Server &get_options() const { return server_opt; }

	ServerLogger lg;

private:
	using Tcp = boost::asio::ip::tcp;

	void start_accept();

	boost::asio::io_context &context;
	Tcp::acceptor acceptor;
	const std::shared_ptr<const Options> global_opt;
	const Options::Server &server_opt;
	const std::shared_ptr<const Router> rout;
};
