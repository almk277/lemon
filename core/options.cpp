#include "options.hpp"
#include "config.hpp"
#include <tuple>
#include <unordered_map>

using std::string;

static bool operator==(const Options::Route::Equal& lhs, const Options::Route::Equal& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const Options::Route::Prefix& lhs, const Options::Route::Prefix& rhs)
{
	return lhs.str == rhs.str;
}

static bool operator==(const Options::Route::Regex& lhs, const Options::Route::Regex& rhs)
{
	return lhs.re == rhs.re;
}

static bool operator==(const Options::Route& lhs, const Options::Route& rhs)
{
	auto tie = [](const auto& r) { return std::tie(r.matcher, r.handler); };
	return tie(lhs) == tie(rhs);
}

bool operator==(const Options::Server& lhs, const Options::Server& rhs)
{
	auto tie = [](const auto& s) { return std::tie(s.listen_port, s.routes); };
	return tie(lhs) == tie(rhs);
}

namespace
{
decltype(Options::Route::matcher) parse_matcher(const string& s)
{
	if (s.at(0) == '=')
		return Options::Route::Equal{ s.substr(1) };
	if (s.at(0) == '~')
		return Options::Route::Regex{ s.substr(1) };
	return Options::Route::Prefix{ s };
}

decltype(Options::LogTypes::MessagesLog::dest) parse_msg_dest(const string& s)
{
	if (s == "console")
		return Options::LogTypes::Console{};
	return Options::LogTypes::File{ s };
}

decltype(Options::LogTypes::AccessLog::dest) parse_access_dest(const string& s)
{
	if (s == "null")
		return Options::LogTypes::Null{};
	if (s == "console")
		return Options::LogTypes::Console{};
	return Options::LogTypes::File{ s };
}

Options::LogTypes::Severity parse_severity(const string& s)
{
	using severity = Options::LogTypes::Severity;
	static const std::unordered_map<string, severity> severities = {
		{ "error",   severity::error },
		{ "warning", severity::warning },
		{ "info",    severity::info },
		{ "debug",   severity::debug },
		{ "trace",   severity::trace },
	};

	auto it = severities.find(s);
	if (it == severities.end())
		throw Options::Error{ "unknown severity: " + s };
	return it->second;
}
}

Options::Options():
	servers{
		{8080, 
			{
			{Route::Prefix{"/"}, "Testing"},
		},
		},
	}
{
	// Hardcoded options here
}

#ifndef LEMON_NO_CONFIG
Options::Options(const config::Table& config)
{
	using namespace config;

	if (auto& n_workers_it = config["workers"]; n_workers_it)
		n_workers = n_workers_it.as<Integer>();

	if (auto& headers_size_it = config["headers_size"]; headers_size_it)
		headers_size = headers_size_it.as<Integer>();

	if (auto& log_messages_it = config["log.messages"]; log_messages_it)
		log.messages.dest = parse_msg_dest(log_messages_it.as<string>());

	if (auto& log_level_it = config["log.level"]; log_level_it)
		log.messages.level = parse_severity(log_level_it.as<string>());

	if (auto& log_access_it = config["log.access"]; log_access_it)
		log.access.dest = parse_access_dest(log_access_it.as<string>());

	for (auto& srv_val : config.get_all("server")) {
		auto& srv = srv_val->as<Table>();
		auto& s = servers.emplace_back();
		s.listen_port = srv["listen"].as<Integer>();
		auto& routes = srv["route"].as<Table>();
		for (auto& route : routes)
			s.routes.push_back({ parse_matcher(route.key()), route.as<string>() });
	}

	//TODO check unknown keys
}
#endif