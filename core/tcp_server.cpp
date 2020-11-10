#include "tcp_server.hpp"
#include "http_router.hpp"
#include "tcp_client.hpp"

namespace tcp
{
Server::Server(boost::asio::io_context& context, std::shared_ptr<const Options> global_opt,
	const Options::Server& server_opt, const http::RhManager& rhman):
	lg{server_opt.listen_port},
	context{context},
	acceptor{context, Tcp::endpoint{ Tcp::v4(), server_opt.listen_port }},
	global_opt{move(global_opt)},
	server_opt{server_opt},
	router{ std::make_shared<http::Router>(rhman, server_opt.routes) }
{
	lg.debug("server created");

	acceptor.listen();
	start_accept();
}

Server::~Server()
{
	lg.debug("server removed");
}

void Server::start_accept()
{
	//TODO allocate handler in pool
	acceptor.async_accept([this](const boost::system::error_code& ec, Tcp::socket sock)
	{
		if (!ec)
			Client::make(context, std::move(sock), global_opt, router, lg);
		else if (ec == boost::asio::error::operation_aborted)
			return;
		else
			lg.error("accept error: ", ec);

		start_accept();
	});
}
}