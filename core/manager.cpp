#include "manager.hpp"
#include "algorithm.hpp"
#include "logs.hpp"
#include "module_manager.hpp"
#include "module_provider.hpp"
#include "options.hpp"
#include "parameters.hpp"
#include "tcp_server.hpp"
#ifndef LEMON_NO_CONFIG
# include "config.hpp"
# include "config_parser.hpp"
#endif
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <set>
#include <stdexcept>

namespace
{
struct FinishWorker: std::exception
{
	auto what() const noexcept -> const char* override { return "finish_worker"; }
};

auto n_workers_default()
{
	auto n_cores = std::thread::hardware_concurrency();
	return n_cores > 0 ? n_cores : 2u;
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

auto Manager::run() -> void
{
	lg.trace("manager started");

	master_ctx.run();

	lg.trace("manager finished");
}

auto Manager::reinit() -> void
{
	lg.trace("manager reinit");

	post(master_ctx, [this] { init(); });
}

auto Manager::init() -> void
{
	lg.debug("initializing manager");

	try {
		config::Table* p_config = nullptr;
#ifdef LEMON_NO_CONFIG
		opts = std::make_shared<options>();
#else
		auto config_file = std::make_shared<config::File>(config_path);
		auto config = parse(config_file);
		p_config = &config;
		auto opts = std::make_shared<Options>(config);
#endif
		
		if (opts->servers.empty())
			throw std::runtime_error{ "no servers configured" };

		logs::init(*opts);
		init_modules(p_config);
		init_servers(opts);
		init_workers(*opts);
	} catch (std::exception& e) {
		lg.error("init: ", e.what());
		if (srv.empty())
			throw;
		lg.warning("init: reconfiguration failed, fallback");
	}
}

auto Manager::init_modules(const config::Table* config) -> void
{
	auto builtin = BuiltinModules{};
	auto providers = std::vector<ModuleProvider*>{ &builtin };
	module_manager = std::make_shared<ModuleManager>(providers, config, lg);
	if (module_manager->handler_count() == 0)
		throw std::runtime_error{ "no request handlers loaded" };
}

auto Manager::init_servers(const std::shared_ptr<const Options>& opts) -> void
{
	lg.trace("init_servers");

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
		srv.push_back(std::make_unique<tcp::Server>(worker_ctx, opts, s, module_manager));
}

auto Manager::init_workers(const Options& opts) -> void
{
	lg.trace("init_workers");

	const auto current_n_workers = n_workers;
	const auto required_n_workers = opts.n_workers.value_or_eval(n_workers_default);
	lg.trace("current worker number: ", current_n_workers, ", required: ", required_n_workers);

	BOOST_ASSERT(required_n_workers > 0);

	for (auto i = current_n_workers; i < required_n_workers; ++i)
		add_worker();
	for (auto i = current_n_workers; i > required_n_workers; --i)
		remove_worker();

	n_workers = required_n_workers;
}

auto Manager::add_worker() -> void
{
	lg.trace("add worker");

	auto worker = std::thread{ [this] { run_worker(); } };
	auto id = worker.get_id();
	BOOST_ASSERT(workers.find(id) == workers.end());
	workers[id] = move(worker);
}

auto Manager::remove_worker() -> void
{
	lg.trace("remove worker");

	BOOST_ASSERT(!workers.empty());
	post(worker_ctx, [] { throw FinishWorker{}; });
}

auto Manager::run_worker() noexcept -> void
{
	GlobalLogger lg;
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

auto Manager::finalize_worker(std::thread::id id) -> void
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
