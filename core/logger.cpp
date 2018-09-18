#include "logger.hpp"
#include "logger_imp.hpp"

inline logger_imp *impl(logger *lg)
{
	return static_cast<logger_imp*>(lg);
}

bool logger::open(severity s)
{
	extern severity log_severity_level;

	if (s > log_severity_level)
		return false;
	
	impl(this)->open_message(s);
	return true;
}

void logger::push(base_printer &c) noexcept
{
	impl(this)->push(c);
}

template <> void logger::log1<>()
{
	impl(this)->finalize();
}
