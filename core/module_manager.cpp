#include "module_manager.hpp"
#include "base_request_handler.hpp"
#include "config.hpp"
#include "logger_imp.hpp"
#include "module.hpp"
#include "module_provider.hpp"
#include <boost/range/algorithm/find_if.hpp>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace
{
auto verify(const ModuleProvider::List& modules)
{
	using namespace std::string_literals;

	std::set<string_view> names;
	for (std::size_t i = 0; i < modules.size(); ++i) {
		auto& m = modules[i];
		auto name = m->get_name();
		if (names.count(name) != 0)
			throw std::runtime_error{ "module #"s + std::to_string(i)
				+ ": name conflict, already exists module with name: "s + std::string{ name } };
		names.insert(name);
	}
}
}

ModuleManager::ModuleManager(const std::vector<ModuleProvider*>& providers, const config::Table* config, GlobalLogger& lg):
	lg{ lg }
{
	ModuleProvider::List all_modules;
	for (auto provider : providers) {
		auto ms = provider->get();
		lg.trace("module provider: ", provider->get_name());
		move(ms.begin(), ms.end(), back_inserter(all_modules));
	}
	
	verify(all_modules);

	if (config)
		load_from_config(all_modules, *config);
	else
		load_all(all_modules);
}

auto ModuleManager::add(std::unique_ptr<Module> module, const config::Table* config) -> void
{
	using namespace std::string_literals;
	
	auto name = module->get_name();
	try {
		lg.trace("init module: ", name);
		GlobalModuleLoggerGuard m_lg{ lg, name };
		for (auto& handler : module->init(lg, config)) {
			auto h_name = handler->get_name();
			auto [it, inserted] = handlers.emplace(h_name, handler);
			if (!inserted)
				throw std::runtime_error{ "already exists request handler with name: " + std::string{ h_name } };
			lg.trace("enable request handler: ", h_name);
		}
		modules.push_back(move(module));
		lg.trace("init module done: ", name);
	} catch (std::exception& e) {
		throw std::runtime_error{ "module "s + std::string{ name } + ": "s + e.what() };
	}
}

auto ModuleManager::get_handler(string_view name) const noexcept -> std::shared_ptr<BaseRequestHandler>
{
	auto found = handlers.find(name);
	return found == handlers.end() ? nullptr : found->second;
}

auto ModuleManager::handler_count() const noexcept -> int
{
	return static_cast<int>(handlers.size());
}

auto ModuleManager::load_from_config(ModuleList& all_modules, const config::Table& config) -> void
{
	auto enable = [this, &all_modules](string_view name, const config::Table* config)
	{
		auto m_it = boost::find_if(all_modules, [name](auto& m)
		{
			return name == m->get_name();
		});
		if (m_it == all_modules.end())
			throw std::runtime_error{ "no such module: " + std::string(name) };
		auto m = move(*m_it);
		all_modules.erase(m_it);
		add(move(m), config);
	};

	//TODO per-server configuration
	for (auto& m_prop : config.get_all("module"))
		if (m_prop->is<config::String>()) {
			auto& name = m_prop->as<config::String>();
			enable(name, nullptr);
		}
		else if (m_prop->is<config::Table>()) {
			auto& m = m_prop->as<config::Table>();
			auto& name = m["name"].as<config::String>();
			auto& enabled_prop = m["enabled"];
			auto enabled = enabled_prop ? enabled_prop.as<config::Boolean>() : true;
			if (enabled)
				enable(name, &m);
			else
				lg.debug("module ", name, ": disabled");
		}
	//TODO else throw error
}

auto ModuleManager::load_all(ModuleList& all_modules) -> void
{
	for (auto& m : all_modules)
		add(move(m), nullptr);
}