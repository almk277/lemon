#include "stub_logger.hpp"

bool logger::open(severity)
{
	return true;
}

void logger::push(base_printer&) noexcept {}

template<> void logger::log1<>() {}

stub_logger slg;