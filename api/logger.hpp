#pragma once
#include <utility>
#include <ostream>

#ifndef LEMON_LOG_LEVEL
#  define LEMON_LOG_LEVEL 3
#endif

static_assert(LEMON_LOG_LEVEL >= 1 && LEMON_LOG_LEVEL <= 5,
	"log level should be in [1(error), 5(trace)]");

class Logger
{
public:
	enum class Severity
	{
		error   = 1,
		warning = 2,
		info    = 3,
		debug   = 4,
		trace   = 5,
	};

	static constexpr Severity severity_barrier = Severity{ LEMON_LOG_LEVEL };

	template <typename... Args> void error(Args&&... args);
	template <typename... Args> void warning(Args&&... args);
	template <typename... Args> void info(Args&&... args);
	template <typename... Args> void debug(Args&&... args);
	template <typename... Args> void trace(Args&&... args);

	template <Severity S, typename... Args> void message(Args&&... args);

	struct BasePrinter
	{
		virtual ~BasePrinter() = default;
		virtual void print(std::ostream& stream) = 0;

		BasePrinter* next{};
	};
	template <typename T> class Printer;

	Logger(Logger&& rhs) = delete;

	Logger& operator=(const Logger& rhs) = delete;
	Logger& operator=(Logger&& rhs) = delete;

protected:
	Logger() = default;
	Logger(const Logger& rhs) = default;
	~Logger() = default;

	template <typename... Args> void log1(Args&&...);
	template <typename T, typename... Args>
	void log1(T&& a, Args&&... args);

private:
	bool open(Severity s);
	void push(BasePrinter& c) noexcept;
};

template <typename... Args>
void Logger::error(Args&&... args)
{
	message<Severity::error>(std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::warning(Args&&... args)
{
	message<Severity::warning>(std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::info(Args&&... args)
{
	message<Severity::info>(std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::debug(Args&&... args)
{
	message<Severity::debug>(std::forward<Args>(args)...);
}

template <typename... Args>
void Logger::trace(Args&&... args)
{
	message<Severity::trace>(std::forward<Args>(args)...);
}

template <Logger::Severity S, typename... Args>
void Logger::message(Args&&... args)
{
	if constexpr(S <= severity_barrier)
	{
		if (open(S))
		{
			log1(std::forward<Args>(args)...);
		}
	}
}

template <typename T, typename... Args>
void Logger::log1(T&& a, Args&&... args)
{
	Printer<T> p{a};
	push(p);
	log1(std::forward<Args>(args)...);
}

template <> void Logger::log1<>();

template<typename T>
class Logger::Printer : public BasePrinter
{
public:
	explicit Printer(const T& n) noexcept: n{ n } {}
	void print(std::ostream& stream) override
	{
		stream << n;
	}
private:
	const T& n;
};