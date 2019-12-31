#pragma once

#include <boost/core/noncopyable.hpp>
#include <boost/optional/optional.hpp>
#include <boost/variant/variant.hpp>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <list>
#include <stdexcept>

namespace config
{
class table;
}

class options: boost::noncopyable
{
public:
	struct error: std::runtime_error
	{
		explicit error(const std::string &s):
			runtime_error("options error: " + s) {}
	};

	struct log_types
	{
		enum class severity {
			error,
			warning,
			info,
			debug,
			trace,
		};

		struct null {};
		struct console {};
		struct file { std::string path; };

		struct messages_log
		{
			boost::variant<console, file> dest;
			severity level = severity::info;
		};
		struct access_log
		{
			boost::variant<console, file, null> dest;
		};
		struct logs
		{
			messages_log messages;
			access_log access;
		};
	};

	struct route
	{
		struct equal { std::string str; };
		struct prefix { std::string str; };
		struct regex { std::string re; };

		boost::variant<equal, prefix, regex> matcher;
		std::string handler;
	};

	using route_list = std::list<route>;

	struct server
	{
		std::uint16_t listen_port = 80;
		route_list routes;
	};

	explicit options(const config::table &config);

	boost::optional<unsigned> n_workers;
	std::size_t headers_size = 4 * 1024;
	log_types::logs log = {
		{ log_types::console{}, log_types::severity::debug },
		{ log_types::console{} }
	};
	std::vector<server> servers;
};

bool operator==(const options::server &lhs, const options::server &rhs);