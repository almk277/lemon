#pragma once

#include "logger.hpp"
#include "task_ident.hpp"
#include "string_view.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/asio/ip/address.hpp>

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
		boost::log::attribute_name server;
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
		extern bool log_access_enabled;
		if (log_access_enabled) {
			open_access();
			log1(std::forward<Args>(args)...);
		}
#endif
	}

	attribute add(const boost::log::attribute_name &name,
		const boost::log::attribute &attr);
	void open_message(severity s);
	void open_access();
	void push(base_printer &c) noexcept;
	void finalize();

	static const attr_name_type attr_name;

protected:
	virtual void insert_attributes() = 0;

	boost::log::attribute_value_set &attributes() noexcept
	{
		return rec.attribute_values();
	}

private:
	void open_internal();

	boost::log::sources::logger lg;
	boost::log::record rec;
	message_type msg{};
};

struct common_logger: logger_imp
{
	void insert_attributes() override {}
};

struct server_logger: common_logger
{
	explicit server_logger(std::uint16_t port) noexcept:
		port{port}
	{}

	void insert_attributes() override;

	const std::uint16_t port;
};

struct client_logger: server_logger
{
	client_logger(const server_logger &logger, boost::asio::ip::address address) noexcept:
		server_logger{logger.port},
		address{std::move(address)}
	{}

	void insert_attributes() override;

	const boost::asio::ip::address address;
};

struct task_logger: client_logger
{
	task_logger(const client_logger &logger, task_ident id) noexcept:
		client_logger{ logger, logger.address },
		id{ id }
	{}

	void insert_attributes() override;

	const task_ident id;
	string_view module_name;
};

struct module_logger_guard: boost::noncopyable
{
	module_logger_guard(task_logger &lg, string_view name) noexcept:
		lg{lg}
	{
		lg.module_name = name;
	}

	~module_logger_guard()
	{
		lg.module_name = {};
	}

private:
	task_logger &lg;
};