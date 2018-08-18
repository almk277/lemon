#include "stub_logger.hpp"

bool logger::open(channel, severity)
{
	return true;
}

void logger::push(base_printer&) noexcept {}

template<> void logger::log1<>() {}

void logger::attach_time() {}

stub_logger slg;