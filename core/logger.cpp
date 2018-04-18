#include "logger.hpp"
#include "logger_imp.hpp"

inline logger_imp *impl(logger *lg)
{
	return static_cast<logger_imp*>(lg);
}

void logger::attach_time()
{
	impl(this)->attach_time();
}

bool logger::open(channel c, severity s)
{
	return impl(this)->open(c, s);
}

void logger::push(base_printer &c) noexcept
{
	impl(this)->push(c);
}

void logger::finalize()
{
	impl(this)->finalize();
}

template <> void logger::log1<>()
{
	finalize();
}
