#include "manager.hpp"
#include "options.hpp"
#include "server.hpp"
#include "logs.hpp"
#include "rh_manager.hpp"
#include "algorithm.hpp"
#include "config_parser.hpp"
#include "config.hpp"
#include "modules/testing.hpp"
#include "modules/static_file.hpp"
#include <boost/assert.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <stdexcept>
#include <set>

#ifndef LEMON_CONFIG_PATH
# define LEMON_CONFIG_PATH ./lemon.ini
#endif

struct finish_worker: std::exception
{
	const char *what() const noexcept override { return "finish_worker"; }
};

static unsigned n_workers_default()
{
	auto n_cores = std::thread::hardware_concurrency();
	return n_cores > 0 ? n_cores : 2;
}

manager::manager(const parameters &params):
	master_work{master_srv},
	worker_work{worker_srv},
	quit_signals{master_srv, SIGTERM, SIGINT},
	params{params}
{
	quit_signals.async_wait([this](const boost::system::error_code&, int sig)
	{
		lg.info("caught termination signal #", sig);
		worker_srv.stop();
		if (workers.empty())
			master_srv.stop();
	});

	init();

	lg.trace("manager created");
}

manager::~manager()
{
	lg.trace("manager destroyed");
}

void manager::run()
{
	lg.trace("manager started");

	master_srv.run();

	lg.trace("manager finished");
}

void manager::reinit()
{
	lg.trace("manager reinit");

	master_srv.post([this] { init(); });
}

void manager::init()
{
	lg.debug("initializing manager");

	std::shared_ptr<options> opts;

	try {
		auto config_path = boost::filesystem::path{ BOOST_STRINGIZE(LEMON_CONFIG_PATH) };
		auto config_file = std::make_shared<config::file>(config_path);
		auto config = parse(config_file);
		opts = std::make_shared<options>(config);
		if (opts->servers.empty())
			throw std::runtime_error("no servers configured");
	} catch (std::exception& e) {
		lg.error("init: ", e.what());
		if (srv.empty())
			throw;
		lg.warning("init: reconfiguration failed, fallback");
	}

	if (opts) {
		logs::init(*opts);
		init_servers(opts);
		init_workers(opts);
	}
}

void manager::init_servers(const std::shared_ptr<const options> &opts)
{
	lg.trace("init_servers");

	rh_manager rhman;
	rhman.add(std::make_shared<rh_testing>());
	rhman.add(std::make_shared<rh_static_file>());

	std::set<decltype(options::server::listen_port)> running_servers;
	for (auto it = srv.begin(); it != srv.end();)
	{
		auto &server = *it;
		auto server_ok = contains(opts->servers, server->get_options());
		if (server_ok) {
			running_servers.insert(server->get_options().listen_port);
			++it;
		} else {
			server->lg.trace("server not in config");
			it = srv.erase(it);
		}
	}

	auto not_running = [&running_servers](const auto &opt)
	{
		return !contains(running_servers, opt.listen_port);
	};
	for (auto &s : opts->servers | boost::adaptors::filtered(not_running))
		srv.push_back(std::make_unique<server>(worker_srv, opts, s, rhman));
}

void manager::init_workers(const std::shared_ptr<const options> &opts)
{
	lg.trace("init_workers");

	const auto current_n_workers = n_workers;
	const auto required_n_workers = opts->n_workers.value_or_eval(n_workers_default);
	lg.trace("current worker number: ", current_n_workers, ", required: ", required_n_workers);

	BOOST_ASSERT(required_n_workers > 0);

	for (auto i = current_n_workers; i < required_n_workers; ++i)
		add_worker();
	for (auto i = current_n_workers; i > required_n_workers; --i)
		remove_worker();

	n_workers = required_n_workers;
}

void manager::add_worker()
{
	lg.trace("add worker");

	auto worker = std::thread{ [this] { run_worker(); } };
	auto id = worker.get_id();
	BOOST_ASSERT(workers.find(id) == workers.end());
	workers[id] = move(worker);
}

void manager::remove_worker()
{
	lg.trace("remove worker");

	BOOST_ASSERT(!workers.empty());
	worker_srv.post([] { throw finish_worker{}; });
}

void manager::run_worker() noexcept
{
	common_logger lg;
	auto n = std::this_thread::get_id();

	lg.debug("started worker thread #", n);

	while (!worker_srv.stopped()) {
		try {
			worker_srv.run();
		} catch (finish_worker&) {
			break;
		} catch (std::exception &e) {
			lg.error(e.what());
		} catch (...) {
			lg.error("unknown worker error");
		}
	}

	lg.debug("finished worker thread #", n);

	master_srv.post([this, n] { finalize_worker(n); });
}

void manager::finalize_worker(std::thread::id id)
{
	lg.debug("finalize worker #", id);

	auto it = workers.find(id);
	if (it == workers.end()) {
		lg.warning("finalization: worker #", id, " not found");
	} else {
		it->second.join();
		workers.erase(it);
		lg.trace("finalized worker #", id, ", ", workers.size(), " workers left");

		if (worker_srv.stopped() && workers.empty())
			master_srv.stop();
	}
}
