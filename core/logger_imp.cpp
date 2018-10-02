#include "logger_imp.hpp"
#include <boost/log/attributes/attribute_value_impl.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/clock.hpp>

using boost::log::attributes::make_attribute_value;

const logger_imp::attr_name_type logger_imp::attr_name{};
const boost::log::attributes::local_clock clock_attr{};

auto logger_imp::add(const boost::log::attribute_name &name,
	const boost::log::attribute &attr) -> attribute
{
	auto r = lg.add_attribute(name, attr);
	return r.first;
}

void logger_imp::open_internal()
{
	rec = lg.open_record();
	BOOST_ASSERT(static_cast<bool>(rec));

	insert_attributes();
	msg = {};
}

void logger_imp::open_message(severity s)
{
	open_internal();
	attributes().insert(attr_name.severity, make_attribute_value(s));
}

void logger_imp::open_access()
{
	open_internal();
	attributes().insert(attr_name.time, clock_attr.get_value());
}

void logger_imp::push(base_printer &c) noexcept
{
	const auto p = &c;
	if (!msg.first)
		msg.first = p;
	if (msg.last)
		msg.last->next = p;
	msg.last = p;
}

void logger_imp::finalize()
{
	attributes().insert(attr_name.lazy_message, make_attribute_value(msg));
	lg.push_record(std::move(rec));
}

void client_logger::insert_attributes()
{
	common_logger::insert_attributes();

	attributes().insert(attr_name.address, make_attribute_value(address));
}

void task_logger::insert_attributes()
{
	client_logger::insert_attributes();

	attributes().insert(attr_name.task, make_attribute_value(id));
	if (!module_name.empty())
		attributes().insert(attr_name.module, make_attribute_value(module_name));
}

logger_imp::attr_name_type::attr_name_type():
	lazy_message{"LazyMessage"},
	time{"TimeStamp"},
	severity{"Severity"},
	task{"TaskID"},
	address{"ClientAddress"},
	module{"Module"}
{
}
