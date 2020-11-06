#include "logs.hpp"
#include "logger_imp.hpp"
#include "options.hpp"
#include "string_view.hpp"
#include "task_ident.hpp"
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/expressions/message.hpp>
#include <boost/log/expressions/formatters/if.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/predicates/has_attr.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/phoenix/operator.hpp>
#include <stdexcept>
#include <iostream>
#include <array>

Logger::Severity log_severity_level = Logger::Severity::info;
bool log_access_enabled = true;

constexpr std::array severity_strings = {
	"!!! "sv,
	"ERR "sv,
	"WRN "sv,
	"INF "sv,
	"DBG "sv,
	"TRC "sv,
};

static std::ostream &operator<<(std::ostream& s, Logger::Severity sev)
{
	return s << severity_strings[static_cast<int>(sev)];
}

static std::ostream &operator<<(std::ostream& s, LoggerImp::Message msg)
{
	for (auto c = msg.first; c; c = c->next)
		c->print(s);
	return s;
}

namespace
{
static_assert(severity_strings[static_cast<int>(Logger::Severity::error)] == "ERR "sv);
static_assert(severity_strings[static_cast<int>(Logger::Severity::trace)] == "TRC "sv);

LoggerImp::Severity convert(Options::LogTypes::Severity s)
{
	using opt = Options::LogTypes::Severity;
	using lg = Logger::Severity;
	switch (s) {
	case opt::trace: return lg::trace;
	case opt::debug: return lg::debug;
	case opt::info: return lg::info;
	case opt::warning: return lg::warning;
	case opt::error: return lg::error;
	default: BOOST_ASSERT(0);
	}
	return lg::error;
}

BOOST_LOG_ATTRIBUTE_KEYWORD(kw_lazymessage, LoggerImp::attr_name.lazy_message,
	LoggerImp::Message)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_severity, LoggerImp::attr_name.severity, Logger::Severity)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_server, LoggerImp::attr_name.server, std::uint16_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_taskid, LoggerImp::attr_name.task, TaskIdent)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_address, LoggerImp::attr_name.address,
	boost::asio::ip::address)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_module, LoggerImp::attr_name.module, string_view)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_time, LoggerImp::attr_name.time,
	boost::log::attributes::local_clock::value_type)

template <typename Filter, typename Fmt>
struct LogAdder
{
	LogAdder(Filter filter, Fmt fmt) : filter{ filter }, fmt { fmt } {}

	bool operator()(const Options::LogTypes::Null&) const { return false; }
	bool operator()(const Options::LogTypes::Console&) const
	{
		boost::log::add_console_log(std::clog, filter, fmt);
		return true;
	}
	bool operator()(const Options::LogTypes::File& f) const
	{
		using namespace boost::log;

		add_file_log(
			keywords::file_name = f.path,
			keywords::auto_flush = true,
			filter,
			fmt
		);
		return true;
	}
	Filter filter;
	Fmt fmt;
};

bool add_messages_sink(const Options::LogTypes::MessagesLog& log)
{
	using namespace boost::log;

	return visit(LogAdder{
		keywords::filter =
			!has_attr(kw_time),
		keywords::format = expressions::stream
			<< kw_severity
			<< if_(has_attr(kw_server))
			[
				expressions::stream << "$" << kw_server << " "
			]
			<< if_(has_attr(kw_address))
			[
				expressions::stream << kw_address << " "
			]
			<< if_(has_attr(kw_taskid))
			[
				expressions::stream << "#" << kw_taskid << " "
			]
			<< if_(has_attr(kw_module))
			[
				expressions::stream << "[" << kw_module << "] "
			]
			<< kw_lazymessage
		}, log.dest);
}

bool add_access_sink(const Options::LogTypes::AccessLog& log)
{
#ifndef LEMON_NO_ACCESS_LOG
	using namespace boost::log;

	return visit(LogAdder{
		keywords::filter =
			has_attr(kw_time),
		keywords::format = expressions::stream
			<< format_date_time(kw_time, "%y-%m-%d %T") << " "
			<< kw_server << " "
			<< kw_address << " "
			<< kw_lazymessage
		}, log.dest);
#else
	return false;
#endif
}
}

void logs::preinit()
{
	const Options::LogTypes::MessagesLog startup_log{
		Options::LogTypes::Console{},
		Options::LogTypes::Severity::info
	};
	add_messages_sink(startup_log);
}

void logs::init(const Options& opt)
{
	using namespace boost::log;

	log_severity_level = convert(opt.log.messages.level);

	if (log_severity_level > Logger::severity_barrier)
		throw std::runtime_error{ "requested log level ("
			+ std::to_string(static_cast<int>(log_severity_level))
			+ ") is too high, supported: "
			+ std::to_string(LEMON_LOG_LEVEL)};

	core::get()->remove_all_sinks();

	add_messages_sink(opt.log.messages);
	log_access_enabled = add_access_sink(opt.log.access);
}