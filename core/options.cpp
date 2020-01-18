#include "options.hpp"
#include "config.hpp"
#include <tuple>
#include <unordered_map>

using std::string;

static bool operator==(const options::route::equal& lhs, const options::route::equal& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const options::route::prefix& lhs, const options::route::prefix& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const options::route::regex& lhs, const options::route::regex& rhs)
{
	return lhs.re == rhs.re;
}

static bool operator==(const options::route& lhs, const options::route& rhs)
{
	auto tie = [](const auto& r) { return std::tie(r.matcher, r.handler); };
	return tie(lhs) == tie(rhs);
}

bool operator==(const options::server &lhs, const options::server &rhs)
{
	auto tie = [](const auto &s) { return std::tie(s.listen_port, s.routes); };
	return tie(lhs) == tie(rhs);
}

namespace
{
decltype(options::route::matcher) parse_matcher(const string &s)
{
	if (s.at(0) == '=')
		return options::route::equal{ s.substr(1) };
	if (s.at(0) == '~')
		return options::route::regex{ s.substr(1) };
	return options::route::prefix{ s };
}

decltype(options::log_types::messages_log::dest) parse_msg_dest(const string &s)
{
	if (s == "console")
		return options::log_types::console{};
	return options::log_types::file{ s };
}

decltype(options::log_types::access_log::dest) parse_access_dest(const string &s)
{
	if (s == "null")
		return options::log_types::null{};
	if (s == "console")
		return options::log_types::console{};
	return options::log_types::file{ s };
}

options::log_types::severity parse_severity(const string &s)
{
	using severity = options::log_types::severity;
	static const std::unordered_map<string, severity> severities = {
		{ "error",   severity::error },
		{ "warning", severity::warning },
		{ "info",    severity::info },
		{ "debug",   severity::debug },
		{ "trace",   severity::trace },
	};

	auto it = severities.find(s);
	if (it == severities.end())
		throw options::error{ "unknown severity: " + s };
	return it->second;
}
}

options::options():
	servers{
		{8080, 
			{
			{route::prefix{"/"}, "Testing"},
		},
		},
	}
{
	// Hardcoded options here
}

#ifndef LEMON_NO_CONFIG
options::options(const config::table &config)
{
	using namespace config;

	if (auto &n_workers_it = config["workers"]; n_workers_it)
		n_workers = n_workers_it.as<integer>();

	if (auto &headers_size_it = config["headers_size"]; headers_size_it)
		headers_size = headers_size_it.as<integer>();

	if (auto &log_messages_it = config["log.messages"]; log_messages_it)
		log.messages.dest = parse_msg_dest(log_messages_it.as<string>());

	if (auto &log_level_it = config["log.level"]; log_level_it)
		log.messages.level = parse_severity(log_level_it.as<string>());

	if (auto &log_access_it = config["log.access"]; log_access_it)
		log.access.dest = parse_access_dest(log_access_it.as<string>());

	for (auto &srv_val : config.get_all("server")) {
		auto &srv = srv_val->as<table>();
		auto &s = servers.emplace_back();
		s.listen_port = srv["listen"].as<integer>();
		auto &routes = srv["route"].as<table>();
		for (auto &route : routes)
			s.routes.push_back(options::route
				{ parse_matcher(route.key()), route.as<string>() });
	}

	//TODO check unknown keys
}
#endif