#pragma once

#include <utility>
#include <ostream>

#ifndef LEMON_LOG_LEVEL
#  define LEMON_LOG_LEVEL 3
#endif

//BUG should be in [0, 4]
static_assert(LEMON_LOG_LEVEL >= 1 && LEMON_LOG_LEVEL <= 5,
	"log level should be in [1(error), 5(trace)]");

class logger
{
public:
	enum class severity
	{
		error,
		warning,
		info,
		debug,
		trace,
	};

	static constexpr severity severity_barrier = static_cast<severity>(LEMON_LOG_LEVEL);

	template <typename ...Args> void error(Args ...args);
	template <typename ...Args> void warning(Args ...args);
	template <typename ...Args> void info(Args ...args);
	template <typename ...Args> void debug(Args ...args);
	template <typename ...Args> void trace(Args ...args);

	template <severity S, typename ...Args> void message(Args ...args);

	struct base_printer
	{
		virtual ~base_printer() = default;
		virtual void print(std::ostream &stream) = 0;

		base_printer *next{};
	};
	template <typename T> class printer;

	logger(logger &&rhs) = delete;

	logger &operator=(const logger &rhs) = delete;
	logger &operator=(logger &&rhs) = delete;

protected:
	logger() = default;
	logger(const logger &rhs) = default;
	~logger() = default;

	template <typename ...Args> void log1(Args ...);
	template <typename T, typename ...Args>
	void log1(T &&a, Args ...args);

private:
	bool open(severity s);
	void push(base_printer &c) noexcept;
};

template <typename ... Args>
void logger::error(Args... args)
{
	message<severity::error>(std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::warning(Args... args)
{
	message<severity::warning>(std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::info(Args... args)
{
	message<severity::info>(std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::debug(Args... args)
{
	message<severity::debug>(std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::trace(Args... args)
{
	message<severity::trace>(std::forward<Args>(args)...);
}

template <logger::severity S, typename ... Args>
void logger::message(Args ... args)
{
	if constexpr(S <= severity_barrier)
	{
		if (open(S))
		{
			log1(std::forward<Args>(args)...);
		}
	}
}

template <typename T, typename ... Args>
void logger::log1(T &&a, Args... args)
{
	printer<T> p{a};
	push(p);
	log1(std::forward<Args>(args)...);
}

template <> void logger::log1<>();

template<typename T>
class logger::printer : public base_printer
{
public:
	explicit printer(const T &n) noexcept: n{ n } {}
	void print(std::ostream &stream) override
	{
		stream << n;
	}
private:
	const T &n;
};
