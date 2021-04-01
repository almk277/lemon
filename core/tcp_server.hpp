#pragma once
#include "logger_imp.hpp"
#include "options.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/noncopyable.hpp>
#include <memory>

class ModuleManager;

namespace http
{
class Router;
}

namespace tcp
{
class Server: boost::noncopyable
{
public:
	Server(boost::asio::io_context& context, std::shared_ptr<const Options> global_opt,
		const Options::Server& server_opt, std::shared_ptr<ModuleManager> module_manager);
	~Server();

	const Options::Server& get_options() const { return server_opt; }

	ServerLogger lg;

private:
	using Tcp = boost::asio::ip::tcp;

	void start_accept();

	boost::asio::io_context& context;
	Tcp::acceptor acceptor;
	const std::shared_ptr<const Options> global_opt;
	const Options::Server& server_opt;
	const std::shared_ptr<ModuleManager> module_manager;
	const std::shared_ptr<const http::Router> router;
};
}