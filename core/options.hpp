#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <list>
#include <stdexcept>
#include <boost/variant/variant.hpp>
class logger;

class options /*: noncopyable */ {
public:

	struct error: std::runtime_error
	{
		explicit error(const std::string &s):
			runtime_error("options error: " + s) {}
		~error() = default;
	};

	options(int argc, char *argv[], logger &lg);
	~options() = default;

	unsigned n_workers;

	std::uint16_t listen_port;

	std::size_t headers_size;

	struct log_types
	{
		enum class severity {
			trace,
			debug,
			info,
			warning,
			error,
		};

		struct null {};
		struct console {};
		struct file { std::string path; };
		using destination = boost::variant<null, console, file>;
		struct messages_log
		{
			destination dest;
			severity level;
		};
		struct access_log
		{
			destination dest;
		};
		struct logs
		{
			messages_log messages;
			access_log access;
		};
	};
	log_types::logs log;

	struct route
	{
		struct equal { std::string str; };
		struct prefix { std::string str; };
		struct regex { std::string re; };

		boost::variant<equal, prefix, regex> matcher;
		std::string handler;
	};
	std::list<route> routes;
};
