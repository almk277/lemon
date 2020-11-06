#include "logger.hpp"
#include "logger_imp.hpp"

namespace
{
LoggerImp *impl(Logger *lg)
{
	return static_cast<LoggerImp*>(lg);
}
}

bool Logger::open(Severity s)
{
	extern Severity log_severity_level;

	if (s > log_severity_level)
		return false;
	
	impl(this)->open_message(s);
	return true;
}

void Logger::push(BasePrinter &c) noexcept
{
	impl(this)->push(c);
}

template <> void Logger::log1<>()
{
	impl(this)->finalize();
}