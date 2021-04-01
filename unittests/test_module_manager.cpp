#include "base_request_handler.hpp"
#include "logger_imp.hpp"
#include "module_manager.hpp"
#include "module_provider.hpp"
#include "test_config.hpp"
#include <boost/range/algorithm/transform.hpp>
#include <boost/test/unit_test.hpp>

using namespace config::test;

namespace
{
GlobalLogger lg;

struct TestHandler : BaseRequestHandler
{
	TestHandler(string_view name) : name{ name } {}
	
	auto get_name() const noexcept -> string_view override { return name; }

	const string_view name;
};

using InitMap = std::map<string_view, const config::Table*>;
	
struct TestModule : Module
{
	TestModule(string_view name, const std::vector<string_view>& handler_names, InitMap& init_map):
		name{ name },
		init_map{ init_map }
	{
		boost::transform(handler_names, back_inserter(handlers),
			[](auto handler_name) { return std::make_shared<TestHandler>(handler_name); });
	}
	
	auto get_name() const noexcept -> string_view override { return name; }
	
	auto init(Logger&, const config::Table* config) -> HandlerList override
	{
		init_map.emplace(name, config);
		return handlers;
	}

	const string_view name;
	HandlerList handlers;
	InitMap& init_map;
};
	
using ProviderParams = std::vector<std::pair<string_view, std::vector<string_view>>>;

struct TestProvider : ModuleProvider
{
	TestProvider(ProviderParams module_params): module_params(move(module_params)) {}

	auto get_name() const noexcept -> string_view override { return "Test"; }
	
	auto get() -> List override
	{
		List modules;
		for (auto& [name, handler_names] : module_params)
			modules.push_back(std::make_unique<TestModule>(name, handler_names, init_map));
		return modules;
	}

	const ProviderParams module_params;
	InitMap init_map;
};
}

BOOST_AUTO_TEST_SUITE(module_manager_tests)

BOOST_AUTO_TEST_CASE(empty)
{
	ModuleManager manager1{ {}, nullptr, lg };
	BOOST_TEST(manager1.handler_count() == 0);
	
	TestProvider p2{ {} };
	ModuleManager manager2{ { &p2 }, nullptr, lg };
	BOOST_TEST(manager2.handler_count() == 0);

	TestProvider p3{ { {"m0", {}} } };
	ModuleManager manager3{ { &p3 }, nullptr, lg };
	BOOST_TEST(manager3.handler_count() == 0);
}

BOOST_AUTO_TEST_CASE(modules_with_same_name)
{
	TestProvider p1{ { {"m1", {}}, {"m1", {}} } };
	BOOST_CHECK_THROW((ModuleManager{ { &p1 }, nullptr, lg }), std::exception);
	
	TestProvider p2{ { {"m1", {}} } };
	TestProvider p3{ { {"m1", {}} } };
	BOOST_CHECK_THROW((ModuleManager{ { &p2, &p3 }, nullptr, lg }), std::exception);
	
	TestProvider p4{ { {"m1", {}}, {"m2", {}}, {"m3", {}} } };
	TestProvider p5{ { {"m4", {}}, {"m2", {}}, {"m5", {}} } };
	BOOST_CHECK_THROW((ModuleManager{ { &p4, &p5 }, nullptr, lg }), std::exception);
}

BOOST_AUTO_TEST_CASE(handlers_with_same_name)
{
	TestProvider p1{ { {"m1", {"h1", "h1"}} } };
	BOOST_CHECK_THROW((ModuleManager{ { &p1 }, nullptr, lg }), std::exception);
	
	TestProvider p2{ { {"m1", {"h1", "h2"}}, {"m2", {"h2", "h3"}} } };
	BOOST_CHECK_THROW((ModuleManager{ { &p1 }, nullptr, lg }), std::exception);
}

BOOST_AUTO_TEST_CASE(load_all)
{
	TestProvider p{ { {"m1", {"h1", "h2"}}, {"m2", {"h3", "h4"}} } };
	ModuleManager manager{ { &p }, nullptr, lg };

	BOOST_TEST(manager.handler_count() == 4);
	
	for (auto name : { "h1", "h2", "h3", "h4" }) {
		auto handler = manager.get_handler(name);
		BOOST_TEST(handler);
		BOOST_TEST(handler->get_name() == name);
	}
	
	BOOST_TEST(!manager.get_handler("h5"));
}

BOOST_AUTO_TEST_CASE(load_from_config)
{
	TestProvider p{ { {"m1", {"h1"}}, {"m2", {"h2"} }, {"m3", {"h3"} }, {"m4", {"h4"} } } };
	const auto key1 = "mod_prop1";
	const auto value1 = "mod_value1";
	const auto key2 = "mod_prop2";
	const auto value2 = 42;

	auto config = tbl()
		<< prop("module", "m2")
		<< prop("module", tbl()
			<< prop("name", "m3")
			<< prop("enabled", false))
		<< prop("module", tbl()
			<< prop("name", "m4")
			<< prop(key1, value1)
			<< prop(key2, value2));
	
	ModuleManager manager{ { &p }, &config, lg };
	
	BOOST_TEST(p.init_map.size() == 2);
	BOOST_TEST(p.init_map["m2"] == nullptr);
	BOOST_TEST(p.init_map["m4"] != nullptr);
	auto& m_config = *p.init_map["m4"];
	BOOST_TEST(m_config["name"].as<config::String>() == "m4");
	BOOST_TEST(m_config[key1].as<config::String>() == value1);
	BOOST_TEST(m_config[key2].as<config::Integer>() == value2);
}

BOOST_AUTO_TEST_CASE(load_from_config_nonexistent_modle)
{
	TestProvider p{ { {"m1", {}} } };
	auto config = tbl()
		<< prop("module", "m2");
	BOOST_CHECK_THROW((ModuleManager{ { &p }, &config, lg }), std::exception);
}

BOOST_AUTO_TEST_SUITE_END()