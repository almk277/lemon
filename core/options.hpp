#pragma once
#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>
#include <cstddef>
#include <cstdint>
#include <list>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace config
{
class Table;
}

class Options: boost::noncopyable
{
public:
	struct Error: std::runtime_error
	{
		explicit Error(const std::string& s):
			runtime_error("options error: " + s) {}
	};

	struct LogTypes
	{
		enum class Severity {
			error,
			warning,
			info,
			debug,
			trace,
		};

		struct Null {};
		struct Console {};
		struct File { std::string path; };

		struct MessagesLog
		{
			std::variant<Console, File> dest;
			Severity level = Severity::info;
		};
		struct AccessLog
		{
			std::variant<Console, File, Null> dest;
		};
		struct Logs
		{
			MessagesLog messages;
			AccessLog access;
		};
	};

	struct Route
	{
		struct Equal { std::string str; };
		struct Prefix { std::string str; };
		struct Regex { std::string re; };

		std::variant<Equal, Prefix, Regex> matcher;
		std::string handler;
	};

	using RouteList = std::list<Route>;

	struct Server
	{
		std::uint16_t listen_port = 80;
		RouteList routes;
	};

	Options();

#ifndef LEMON_NO_CONFIG
	explicit Options(const config::Table& config);
#endif

	boost::optional<unsigned> n_workers = 1;
	std::size_t headers_size = 4 * 1024;
	LogTypes::Logs log = {
		{ LogTypes::Console{}, LogTypes::Severity::debug },
		{ LogTypes::Console{} }
	};
	std::vector<Server> servers;
};

bool operator==(const Options::Server& lhs, const Options::Server& rhs);