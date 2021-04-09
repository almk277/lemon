#pragma once
#include "module.hpp"
#include "string_view.hpp"
#include <boost/core/noncopyable.hpp>
#include <map>
#include <memory>
#include <vector>

struct BaseRequestHandler;
struct ModuleProvider;
struct GlobalLogger;

class ModuleManager : boost::noncopyable
{
public:
	explicit ModuleManager(const std::vector<ModuleProvider*>& providers,
		const config::Table* config, GlobalLogger& lg);
	auto add(std::unique_ptr<Module> module, const config::Table* config) -> void;
	auto get_handler(string_view name) const noexcept -> std::shared_ptr<BaseRequestHandler>;
	auto handler_count() const noexcept -> int;
	
private:
	using ModuleList = std::vector<std::unique_ptr<Module>>;

	auto load_all(ModuleList all_modules) -> void;
	auto load_from_config(ModuleList all_modules, const config::Table& config) -> void;

	GlobalLogger& lg;
	ModuleList modules;
	std::map<string_view, std::shared_ptr<BaseRequestHandler>> handlers;
};