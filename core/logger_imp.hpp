#pragma once
#include "logger.hpp"
#include "task_ident.hpp"
#include "string_view.hpp"
#include <boost/core/noncopyable.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/asio/ip/address.hpp>

class LoggerImp: public Logger, boost::noncopyable
{
public:
	enum class Channel
	{
		message,
		access,
	};

	using Attribute = boost::log::attribute_set::iterator;

	struct AttrName
	{
		AttrName();

		boost::log::attribute_name lazy_message;
		boost::log::attribute_name time;
		boost::log::attribute_name severity;
		boost::log::attribute_name server;
		boost::log::attribute_name task;
		boost::log::attribute_name address;
		boost::log::attribute_name module;
	};

	struct Message
	{
		BasePrinter* first;
		BasePrinter* last;
	};

	LoggerImp() = default;
	LoggerImp(const LoggerImp& rhs) = delete;
	virtual ~LoggerImp() = default;
	
	LoggerImp& operator=(const LoggerImp&) = delete;

	template <typename... Args> void access(Args&&... args)
	{
#ifndef LEMON_NO_ACCESS_LOG
		extern bool log_access_enabled;
		if (log_access_enabled) {
			open_access();
			log1(std::forward<Args>(args)...);
		}
#endif
	}

	Attribute add(const boost::log::attribute_name& name,
		const boost::log::attribute& attr);
	void open_message(Severity s);
	void open_access();
	void push(BasePrinter& c) noexcept;
	void finalize();

	static const AttrName attr_name;

protected:
	virtual void insert_attributes() = 0;

	boost::log::attribute_value_set& attributes() noexcept
	{
		return rec.attribute_values();
	}

private:
	void open_internal();

	boost::log::sources::logger lg;
	boost::log::record rec;
	Message msg{};
};

struct CommonLogger: LoggerImp
{
	void insert_attributes() override {}
};

struct ServerLogger: CommonLogger
{
	explicit ServerLogger(std::uint16_t port) noexcept:
		port{port}
	{}

	void insert_attributes() override;

	const std::uint16_t port;
};

struct ClientLogger: ServerLogger
{
	ClientLogger(const ServerLogger& logger, boost::asio::ip::address address) noexcept:
		ServerLogger{logger.port},
		address{std::move(address)}
	{}

	void insert_attributes() override;

	const boost::asio::ip::address address;
};

struct TaskLogger: ClientLogger
{
	TaskLogger(const ClientLogger& logger, TaskIdent id) noexcept:
		ClientLogger{ logger, logger.address },
		id{ id }
	{}

	void insert_attributes() override;

	const TaskIdent id;
	string_view module_name;
};

struct ModuleLoggerGuard: boost::noncopyable
{
	ModuleLoggerGuard(TaskLogger& lg, string_view name) noexcept:
		lg{lg}
	{
		lg.module_name = name;
	}

	~ModuleLoggerGuard()
	{
		lg.module_name = {};
	}

private:
	TaskLogger& lg;
};