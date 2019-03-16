#include "manager.hpp"
#include "options.hpp"
#include "server.hpp"
#include "client.hpp"
#include "logs.hpp"
#include "rh_manager.hpp"
#include "modules/testing.hpp"
#include "modules/static_file.hpp"
#include <boost/assert.hpp>
#include <boost/range/counting_range.hpp>

manager::manager(const parameters &params):
	work{service},
	signal_set{service, SIGTERM, SIGINT},
	params(params)
{
	init();

	lg.debug("manager created");
}

manager::~manager()
{
	lg.debug("manager destroyed");
}

void manager::init()
{
	lg.debug("initializing manager");

	auto opts = std::make_shared<options>(params, lg);
	logs::init(*opts);
	n_workers = opts->n_workers;

	rh_manager rhman;
	rhman.add(std::make_shared<rh_testing>());
	rhman.add(std::make_shared<rh_static_file>());

	for (auto &s: opts->servers)
		srv.push_back(std::make_unique<server>(service, opts, s, rhman));
}

void manager::run()
{
	BOOST_ASSERT(n_workers > 0);

	signal_set.async_wait([this](const boost::system::error_code&, int sig)
	{
		lg.info("caught termination signal #", sig);
		service.stop();
	});
	for (auto &s : srv)
		s->run();

	workers.reserve(n_workers);
	for (auto i: boost::counting_range(1u, n_workers))
		workers.emplace_back([this, i] { run_worker(i); });

	run_worker(0);

	lg.debug("stopping threads...");
	for (auto &t: workers)
		t.join();
	lg.debug("threads stopped");
}

void manager::run_worker(unsigned n) noexcept
{
	common_logger lg;

	lg.debug("started worker thread #", n);
	service.run();
	lg.debug("finished worker thread #", n);
}
