#pragma once

#include "logger.hpp"
#include <boost/log/core/record.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>

class logger_imp: public logger
{
public:
	using attribute = boost::log::attribute_set::iterator;

	struct attr_name_type
	{
		attr_name_type();
		boost::log::attribute_name lazy_message;
		boost::log::attribute_name time;
		boost::log::attribute_name severity;
		boost::log::attribute_name channel;
		boost::log::attribute_name task;
		boost::log::attribute_name address;
		boost::log::attribute_name module;
	};

	struct message_type
	{
		base_printer *first;
		base_printer *last;
	};

	logger_imp() = default;
	logger_imp(const logger_imp& rhs) noexcept: lg{ rhs.lg } {}
	logger_imp &operator=(const logger_imp&) = delete;
	~logger_imp() = default;
	
	attribute add(const boost::log::attribute_name &name,
		const boost::log::attribute &attr);
	void remove(attribute &attr);
	void attach_time();
	bool open(channel c, severity s);
	void push(base_printer &c) noexcept;
	void finalize();

	static const attr_name_type attr_name;

private:
	boost::log::sources::severity_channel_logger<severity, channel> lg;
	boost::log::record rec;
	message_type msg;
};