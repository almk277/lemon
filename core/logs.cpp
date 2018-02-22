#include "logs.hpp"
#include "logger_imp.hpp"
#include "options.hpp"
#include "utility.hpp"
#include "task.hpp"
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/expressions/message.hpp>
#include <boost/log/expressions/formatters/if.hpp>
#include <boost/log/expressions/formatters/stream.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/container/vector.hpp>
#include <iostream>

constexpr string_view severity_strings[] = {
	"TRC "_w,
	"DBG "_w,
	"INF "_w,
	"WRN "_w,
	"ERR "_w,
	"    "_w,
};

class channel_states
{
public:
	bool &operator[](logger::channel c)
	{
		const auto idx = static_cast<size_type>(c);
		if (idx >= states.size())
			states.resize(idx + 1);
		return states.at(idx);
	}
	bool operator[](logger::channel c) const
	{
		const auto idx = static_cast<size_type>(c);
		return states[idx];
	}
private:
	using container = boost::container::vector<bool>;
	using size_type = container::size_type;

	container states;
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
	return lg::pass;
}

BOOST_LOG_ATTRIBUTE_KEYWORD(kw_lazymessage, logger_imp::attr_name.lazy_message,
	logger_imp::message_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_time, logger_imp::attr_name.time,
	boost::log::attributes::local_clock::value_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_severity, logger_imp::attr_name.severity, logger::severity)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_channel, logger_imp::attr_name.channel, logger::channel)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_taskid, logger_imp::attr_name.task, task::ident)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_address, logger_imp::attr_name.address,
	boost::asio::ip::address)
BOOST_LOG_ATTRIBUTE_KEYWORD(kw_module, logger_imp::attr_name.module, string_view)

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
			kw_channel == logger::channel::message,
		keywords::format = expressions::stream
			<< kw_severity
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
	using namespace boost::log;

	return boost::apply_visitor(make_adder(
		keywords::filter =
			kw_channel == logger::channel::access,
		keywords::format = expressions::stream
			<< format_date_time(kw_time, "%y-%m-%d %T") << " "
			<< kw_address << " "
			<< kw_lazymessage
		), log.dest);
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

	const auto core = core::get();

	core->remove_all_sinks();

	channel_states channels;
	channels[logger::channel::message] = add_messages_sink(opt.log.messages);
	channels[logger::channel::access] = add_access_sink(opt.log.access);

	core->set_filter([
		message_level = convert(opt.log.messages.level),
		channels = std::move(channels)
	](const attribute_value_set &attrs)
	{
		auto severity = attrs[kw_severity];
		if (severity < message_level)
			return false;
		auto channel = attrs[kw_channel].get();
		return channels[channel];
	});
}
