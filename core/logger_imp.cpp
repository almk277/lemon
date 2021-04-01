#include "logger_imp.hpp"
#include <boost/log/attributes/attribute_value_impl.hpp>
#include <boost/log/attributes/clock.hpp>
#include <boost/log/attributes/constant.hpp>

using boost::log::attributes::make_attribute_value;

const LoggerImp::AttrName LoggerImp::attr_name{};
const boost::log::attributes::local_clock clock_attr{};

auto LoggerImp::add(const boost::log::attribute_name& name,
	const boost::log::attribute& attr) -> Attribute
{
	auto r = lg.add_attribute(name, attr);
	return r.first;
}

void LoggerImp::open_internal()
{
	rec = lg.open_record();
	BOOST_ASSERT(static_cast<bool>(rec));

	insert_attributes();
	msg = {};
}

void LoggerImp::open_message(Severity s)
{
	open_internal();
	attributes().insert(attr_name.severity, make_attribute_value(s));
}

void LoggerImp::open_access()
{
	open_internal();
	attributes().insert(attr_name.time, clock_attr.get_value());
}

void LoggerImp::push(BasePrinter& c) noexcept
{
	const auto p = &c;
	if (!msg.first)
		msg.first = p;
	if (msg.last)
		msg.last->next = p;
	msg.last = p;
}

void LoggerImp::finalize()
{
	attributes().insert(attr_name.lazy_message, make_attribute_value(msg));
	lg.push_record(std::move(rec));
}

void GlobalLogger::insert_attributes()
{
	if (!module_name.empty())
		attributes().insert(attr_name.module, make_attribute_value(module_name));
}

void ServerLogger::insert_attributes()
{
	BaseLogger::insert_attributes();

	attributes().insert(attr_name.server, make_attribute_value(port));
}

void ClientLogger::insert_attributes()
{
	ServerLogger::insert_attributes();

	attributes().insert(attr_name.address, make_attribute_value(address));
}

void TaskLogger::insert_attributes()
{
	ClientLogger::insert_attributes();

	attributes().insert(attr_name.task, make_attribute_value(id));
	if (!handler.empty())
		attributes().insert(attr_name.handler, make_attribute_value(handler));
}

LoggerImp::AttrName::AttrName():
	lazy_message{"LazyMessage"},
	time{"TimeStamp"},
	severity{"Severity"},
	server{"Server"},
	task{"TaskID"},
	address{"ClientAddress"},
	module{"Module"},
	handler{"RequestHandler"}
{
}