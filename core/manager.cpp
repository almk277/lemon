#include "manager.hpp"
#include "algorithm.hpp"
#include "logs.hpp"
#include "options.hpp"
#include "parameters.hpp"
#include "http_rh_manager.hpp"
#include "tcp_server.hpp"
#ifndef LEMON_NO_CONFIG
# include "config.hpp"
# include "config_parser.hpp"
#endif
#include "modules/testing.hpp"
#include "modules/static_file.hpp"
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <set>
#include <stdexcept>

namespace
{
struct FinishWorker: std::exception
{
	const char* what() const noexcept override { return "finish_worker"; }
};

unsigned n_workers_default()
{
	auto n_cores = std::thread::hardware_concurrency();
	return n_cores > 0 ? n_cores : 2;
}
}

Manager::Manager(const Parameters& params):
	master_ctx{ 1 },
	master_work{ make_work_guard(master_ctx) },
	worker_work{ make_work_guard(worker_ctx) },
	quit_signals{ master_ctx, SIGTERM, SIGINT },
	config_path{ params.config_path }
{
	quit_signals.async_wait([this](const boost::system::error_code&, int sig)
	{
		lg.info("caught termination signal #", sig);
		worker_ctx.stop();
		if (workers.empty())
			master_ctx.stop();
	});

	init();

	lg.trace("manager created");
	lg.debug("config_path ", config_path);
}

Manager::~Manager()
{
	lg.trace("manager destroyed");
}

void Manager::run()
{
	lg.trace("manager started");

	master_ctx.run();

	lg.trace("manager finished");
}

void Manager::reinit()
{
	lg.trace("manager reinit");

	post(master_ctx, [this] { init(); });
}

void Manager::init()
{
	lg.debug("initializing manager");

	std::shared_ptr<Options> opts;

	try {
#ifdef LEMON_NO_CONFIG
		opts = std::make_shared<options>();
#else
		auto config_file = std::make_shared<config::File>(config_path);
		auto config = parse(config_file);
		opts = std::make_shared<Options>(config);
#endif
		if (opts->servers.empty())
			throw std::runtime_error{ "no servers configured" };
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

void Manager::init_servers(const std::shared_ptr<const Options>& opts)
{
	lg.trace("init_servers");

	http::RhManager rhman;
	rhman.add(std::make_shared<http::RhTesting>());
	rhman.add(std::make_shared<http::RhStaticFile>());

	std::set<decltype(Options::Server::listen_port)> running_servers;
	for (auto it = srv.begin(); it != srv.end();)
	{
		auto& server = *it;
		auto server_ok = contains(opts->servers, server->get_options());
		if (server_ok) {
			running_servers.insert(server->get_options().listen_port);
			++it;
		} else {
			server->lg.trace("server not in config");
			it = srv.erase(it);
		}
	}

	auto not_running = [&running_servers](const auto& opt)
	{
		return !contains(running_servers, opt.listen_port);
	};
	for (auto& s : opts->servers | boost::adaptors::filtered(not_running))
		srv.push_back(std::make_unique<tcp::Server>(worker_ctx, opts, s, rhman));
}

void Manager::init_workers(const std::shared_ptr<const Options>& opts)
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

void Manager::add_worker()
{
	lg.trace("add worker");

	auto worker = std::thread{ [this] { run_worker(); } };
	auto id = worker.get_id();
	BOOST_ASSERT(workers.find(id) == workers.end());
	workers[id] = move(worker);
}

void Manager::remove_worker()
{
	lg.trace("remove worker");

	BOOST_ASSERT(!workers.empty());
	post(worker_ctx, [] { throw FinishWorker{}; });
}

void Manager::run_worker() noexcept
{
	CommonLogger lg;
	auto n = std::this_thread::get_id();

	lg.debug("started worker thread #", n);

	while (!worker_ctx.stopped()) {
		try {
			worker_ctx.run();
		} catch (FinishWorker&) {
			break;
		} catch (std::exception& e) {
			lg.error(e.what());
		} catch (...) {
			lg.error("unknown worker error");
		}
	}

	lg.debug("finished worker thread #", n);

	post(master_ctx, [this, n] { finalize_worker(n); });
}

void Manager::finalize_worker(std::thread::id id)
{
	lg.debug("finalize worker #", id);

	auto it = workers.find(id);
	if (it == workers.end()) {
		lg.warning("finalization: worker #", id, " not found");
	} else {
		it->second.join();
		workers.erase(it);
		lg.trace("finalized worker #", id, ", ", workers.size(), " workers left");

		if (worker_ctx.stopped() && workers.empty())
			master_ctx.stop();
	}
}