#include "manager.hpp"
#include "client.hpp"
#include "router.hpp"
#include "modules/testing.hpp"
#include <boost/assert.hpp>
#include <boost/range/counting_range.hpp>

manager::manager(options &&opt):
	n_workers{opt.n_workers},
	service{n_workers},
	work{service},
	acceptor{service, tcp::endpoint{tcp::v4(), opt.listen_port}},
	sock{service},
	signal_set{service, SIGTERM, SIGINT},
	opt{opt} //TODO move?
{
	rhman.add(std::make_shared<rh_testing>());
	rout = std::make_shared<router>(rhman, opt);

	signal_set.async_wait([this](const boost::system::error_code&, int sig)
	{
		lg.info("caught termination signal #", sig);
		service.stop();
	});
	acceptor.listen();
	workers.reserve(n_workers);
	lg.debug("manager created");
}

manager::~manager()
{
	lg.debug("stopping threads...");
	service.stop();
	for (auto &t: workers)
		t.join();
	lg.debug("threads stopped");
}

void manager::run()
{
	lg.info("listen on port ", acceptor.local_endpoint().port());
	start_accept();

	BOOST_ASSERT(n_workers > 0);
	for (auto i: boost::counting_range(1u, n_workers))
		add_worker(i);
	service.run();
}

void manager::add_worker(unsigned idx)
{
	workers.emplace_back([this, idx]
	{
		lg.debug("started worker thread #", idx);
		auto n = service.run();
		lg.debug("finished worker thread #", idx, ", events handled: ", n);
	});
}

void manager::start_accept()
{
	//TODO allocate handler in pool
	acceptor.async_accept(
		sock,
		[this](const boost::system::error_code &ec)
		{
			if (ec)
				lg.error("accept error: ", ec);
			else
				client::make(*this, std::move(sock));
			start_accept();
		});
}
