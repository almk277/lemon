#include "logs.hpp"
#include "logger_imp.hpp"
#include "options.hpp"
#include "utility.hpp"
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
#include <boost/variant/static_visitor.hpp>
#include <boost/phoenix/operator.hpp>
#include <stdexcept>
#include <iostream>
#include <array>

logger::severity log_severity_level = logger::severity::info;
bool log_access_enabled = true;

constexpr std::array<string_view, 5> severity_strings = {
	"ERR "_w,
	"WRN "_w,
	"INF "_w,
	"DBG "_w,
	"TRC "_w,
};

static std::ostream &operator<<(std::ostream &s, logger::severity sev)
{
	return s << severity_strings[static_cast<int>(sev)];
}

static std::ostream &operator<<(std::ostream &s, logger_imp::message_type msg)
{
	for (auto c = msg.first; c; c = c->next)
		c->print(s);
	return s;
}

static logger_imp::severity convert(options::log_types::severity s)
{
	using opt = options::log_types::severity;
	using lg = logger::severity;
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

BOOST_LOG_ATTRIBUTE_KEYWORD(kw_lazymessage, logger_imp::attr_name.lazy_message,
	logger_imp::message_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_severity, logger_imp::attr_name.severity, logger::severity)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_server, logger_imp::attr_name.server, std::uint16_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_taskid, logger_imp::attr_name.task, task_ident)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_address, logger_imp::attr_name.address,
	boost::asio::ip::address)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_module, logger_imp::attr_name.module, string_view)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_time, logger_imp::attr_name.time,
	boost::log::attributes::local_clock::value_type)

template <typename Filter, typename Fmt>
struct log_adder: boost::static_visitor<bool>
{
	log_adder(Filter filter, Fmt fmt) : filter{ filter }, fmt { fmt } {}

	bool operator()(const options::log_types::null&) const { return false; }
	bool operator()(const options::log_types::console&) const
	{
		boost::log::add_console_log(std::clog, filter, fmt);
		return true;
	}
	bool operator()(const options::log_types::file &f) const
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

template <typename Filter, typename Fmt>
log_adder<Filter, Fmt> make_adder(Filter filter, Fmt fmt)
{
	return log_adder<Filter, Fmt>{filter, fmt};
}

static bool add_messages_sink(const options::log_types::messages_log &log)
{
	using namespace boost::log;

	return boost::apply_visitor(make_adder(
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
		), log.dest);
}

static bool add_access_sink(const options::log_types::access_log &log)
{
#ifndef LEMON_NO_ACCESS_LOG
	using namespace boost::log;

	return boost::apply_visitor(make_adder(
		keywords::filter =
			has_attr(kw_time),
		keywords::format = expressions::stream
			<< format_date_time(kw_time, "%y-%m-%d %T") << " "
			<< kw_server << " "
			<< kw_address << " "
			<< kw_lazymessage
		), log.dest);
#else
	return false;
#endif
}

void logs::preinit()
{
	const options::log_types::messages_log startup_log{
		options::log_types::console{},
		options::log_types::severity::info
	};
	add_messages_sink(startup_log);
}

void logs::init(const options &opt)
{
	using namespace boost::log;

	log_severity_level = convert(opt.log.messages.level);

	if (log_severity_level > static_cast<logger::severity>(LEMON_LOG_LEVEL))
		throw std::runtime_error{ "requested log level ("
			+ std::to_string(static_cast<int>(log_severity_level))
			+ ") is too high, supported: "
			+ std::to_string(LEMON_LOG_LEVEL)};

	core::get()->remove_all_sinks();

	add_messages_sink(opt.log.messages);
	log_access_enabled = add_access_sink(opt.log.access);
}
