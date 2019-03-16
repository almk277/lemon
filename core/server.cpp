#include "server.hpp"
#include "client.hpp"
#include "router.hpp"

server::server(boost::asio::io_service &service, std::shared_ptr<const options> global_opt,
	const options::server &server_opt, const rh_manager &rhman):
	acceptor{service, tcp::endpoint{ tcp::v4(), server_opt.listen_port }},
	sock{service},
	global_opt{global_opt},
	rout{std::make_shared<router>(rhman, server_opt.routes)},
	lg{server_opt.listen_port}
{
	lg.debug("server created");
}

server::~server()
{
	lg.debug("server removed");
}

void server::run()
{
	acceptor.listen();
	lg.info("listen");
	start_accept();
}

void server::start_accept()
{
	//TODO allocate handler in pool
	acceptor.async_accept(sock, [this](const boost::system::error_code &ec)
	{
		if (ec)
			lg.error("accept error: ", ec);
		else
			client::make(std::move(sock), global_opt, rout, lg);
		start_accept();
	});
}
