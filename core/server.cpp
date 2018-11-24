#include "server.hpp"
#include "client.hpp"

server::server(boost::asio::io_service &service, std::uint16_t port,
	std::shared_ptr<const environment> env):
	acceptor{service, tcp::endpoint{ tcp::v4(), port }},
	sock{service},
	env{move(env)},
	lg{port}
{
}

void server::run()
{
	acceptor.listen();
	lg.info("listen on port ", acceptor.local_endpoint().port());
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
			client::make(std::move(sock), env, lg);
		start_accept();
	});
}
