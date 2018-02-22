#pragma once

#include <utility>
#include <ostream>

class logger
{
public:
	enum class channel
	{
		message,
		access,
	};

	enum class severity
	{
		trace,
		debug,
		info,
		warning,
		error,
		pass
	};

	template <typename ...Args> void trace(Args ...args);
	template <typename ...Args> void debug(Args ...args);
	template <typename ...Args> void info(Args ...args);
	template <typename ...Args> void warning(Args ...args);
	template <typename ...Args> void error(Args ...args);

	template <typename ...Args> void access(Args ...args);

	struct base_printer
	{
		virtual ~base_printer() = default;
		virtual void print(std::ostream &stream) = 0;

		base_printer *next{};
	};
	template <typename T> class printer;

protected:
	logger() = default;
	logger(const logger &rhs) = default;
	~logger() = default;

private:
	bool open(channel c, severity s);
	void push(base_printer &c) noexcept;
	void finalize();
	void attach_time();

	template <typename ...Args> void log(channel c, severity s, Args ...args)
	{
		if (open(c, s))
			log1(std::forward<Args>(args)...);
	}
	template <typename ...Args> void message(severity s, Args ...args)
	{
		log(channel::message, s, std::forward<Args>(args)...);
	}

	template <typename ...Args> void log1(Args ...);
	template <typename T, typename ...Args>
	void log1(T &&a, Args ...args);
};

template <typename ... Args>
void logger::access(Args... args)
{
	if (open(channel::access, severity::pass)) {
		attach_time();
		log1(std::forward<Args>(args)...);
	}
}

template <typename ... Args>
void logger::trace(Args... args)
{
	message(severity::trace, std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::debug(Args... args)
{
	message(severity::debug, std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::info(Args... args)
{
	message(severity::info, std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::warning(Args... args)
{
	message(severity::warning, std::forward<Args>(args)...);
}

template <typename ... Args>
void logger::error(Args... args)
{
	message(severity::error, std::forward<Args>(args)...);
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
