#include "stub_logger.hpp"

bool Logger::open(Severity)
{
	return true;
}

void Logger::push(BasePrinter&) noexcept {}

template<> void Logger::log1<>() {}

StubLogger slg;