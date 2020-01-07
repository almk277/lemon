#include "server.hpp"
#include "client.hpp"
#include "router.hpp"

server::server(boost::asio::io_service &service, std::shared_ptr<const options> global_opt,
	const options::server &server_opt, const rh_manager &rhman):
	lg{server_opt.listen_port},
	service{service},
	acceptor{service, tcp::endpoint{ tcp::v4(), server_opt.listen_port }},
	sock{service},
	global_opt{move(global_opt)},
	server_opt{server_opt},
	rout{std::make_shared<router>(rhman, server_opt.routes)}
{
	lg.debug("server created");

	acceptor.listen();
	start_accept();
}

server::~server()
{
	lg.debug("server removed");
}

void server::start_accept()
{
	//TODO allocate handler in pool
	acceptor.async_accept(sock, [this](const boost::system::error_code &ec)
	{
		if (!ec)
			client::make(service, std::move(sock), global_opt, rout, lg);
		else if (ec == boost::asio::error::operation_aborted)
			return;
		else
			lg.error("accept error: ", ec);

		start_accept();
	});
}
