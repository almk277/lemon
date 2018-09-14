#pragma once

#include "logger.hpp"
#include "task_ident.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/asio/ip/address.hpp>
#include <utility.hpp>

class logger_imp: public logger, boost::noncopyable
{
public:
	enum class channel
	{
		message,
		access,
	};

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
	logger_imp(const logger_imp &rhs) = delete;
	virtual ~logger_imp() = default;
	
	logger_imp &operator=(const logger_imp&) = delete;

	template <typename ...Args> void access(Args ...args)
	{
#ifndef LEMON_NO_ACCESS_LOG
		if (open(channel::access, severity::pass)) {
			attach_time();
			log1(std::forward<Args>(args)...);
		}
#endif
	}

	attribute add(const boost::log::attribute_name &name,
		const boost::log::attribute &attr);
	void remove(const attribute &attr);
	void attach_time();
	bool open(channel c, severity s);
	void push(base_printer &c) noexcept;
	void finalize();

	static const attr_name_type attr_name;

protected:
	virtual void insert_attributes() = 0;

	boost::log::attribute_value_set &attributes()
	{
		return rec.attribute_values();
	}

private:
	boost::log::sources::severity_channel_logger_mt<severity, channel> lg;
	boost::log::record rec;
	message_type msg{};
};

struct common_logger: logger_imp
{
	void insert_attributes() override {}
};

struct client_logger: common_logger
{
	client_logger(const common_logger&, boost::asio::ip::address address):
		address{std::move(address)}
	{}

	void insert_attributes() override;

	const boost::asio::ip::address address;
};

struct task_logger: client_logger
{
	task_logger(const client_logger &logger, task_ident id):
		client_logger{ logger, logger.address },
		id{ id }
	{}

	void insert_attributes() override;

	const task_ident id;
	string_view module_name;
};

struct module_logger_guard: boost::noncopyable
{
	module_logger_guard(task_logger &lg, string_view name):
		lg{lg}
	{
		lg.module_name = name;
	}

	~module_logger_guard()
	{
		lg.module_name.clear();
	}

private:
	task_logger &lg;
};