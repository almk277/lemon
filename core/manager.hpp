#pragma once
#include "logger_imp.hpp"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/core/noncopyable.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <thread>
#include <vector>

class ModuleManager;
struct Parameters;
class Options;

namespace config
{
class Table;
}

namespace tcp
{
class Server;
}

class Manager: boost::noncopyable
{
public:
	explicit Manager(const Parameters& params);
	~Manager();

	auto run() -> void;
	//TODO find a way to call it
	auto reinit() -> void;

private:
	auto init() -> void;
	auto init_modules(const config::Table* config) -> void;
	auto init_servers(const std::shared_ptr<const Options>& opts) -> void;
	auto init_workers(const Options& opts) -> void;
	auto add_worker() -> void;
	auto remove_worker() -> void;
	auto run_worker() noexcept -> void;
	auto finalize_worker(std::thread::id id) -> void;

	boost::asio::io_context master_ctx;
	boost::asio::io_context worker_ctx;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> master_work;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> worker_work;
	boost::asio::signal_set quit_signals;
	std::map<std::thread::id, std::thread> workers;
	unsigned n_workers = 0;
	GlobalLogger lg;
	std::shared_ptr<ModuleManager> module_manager;
	std::vector<std::unique_ptr<tcp::Server>> srv;
	const std::filesystem::path config_path;
};