#include "module_provider.hpp"
#include "modules/static_file.hpp"
#include "modules/testing.hpp"

auto BuiltinModules::get() -> List
{
	// Instantiate all modules you want to embed here
	
	List list;
	list.push_back(std::make_unique<ModuleStaticFile>());
	list.push_back(std::make_unique<ModuleTesting>());

	return list;
}