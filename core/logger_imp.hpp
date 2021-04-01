#pragma once
#include "logger.hpp"
#include "string_view.hpp"
#include "task_ident.hpp"
#include <boost/asio/ip/address.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/sources/logger.hpp>

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
		boost::log::attribute_name handler;
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

struct BaseLogger: LoggerImp
{
	void insert_attributes() override {}
};

struct GlobalLogger : BaseLogger
{
	void insert_attributes() override;
	
	string_view module_name;
};

struct GlobalModuleLoggerGuard
{
	GlobalModuleLoggerGuard(GlobalLogger& lg, string_view name) noexcept:
		lg{ lg }
	{
		lg.module_name = name;
	}

	~GlobalModuleLoggerGuard()
	{
		lg.module_name = {};
	}

private:
	GlobalLogger& lg;
};

struct ServerLogger: BaseLogger
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
	string_view handler;
};

struct HandlerLoggerGuard: boost::noncopyable
{
	HandlerLoggerGuard(TaskLogger& lg, string_view name) noexcept:
		lg{lg}
	{
		lg.handler = name;
	}

	~HandlerLoggerGuard()
	{
		lg.handler = {};
	}

private:
	TaskLogger& lg;
};