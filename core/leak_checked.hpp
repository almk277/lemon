#pragma once
#include <atomic>
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID

template <typename> class LeakChecked {};

#else

template <typename T>
class LeakChecked
{
protected:
	LeakChecked() { ++d.counter; }
	LeakChecked(const LeakChecked&) { ++d.counter; }
	LeakChecked(LeakChecked&&) noexcept { ++d.counter; }
	~LeakChecked() { --d.counter; }
	LeakChecked &operator=(const LeakChecked&) = default;
	LeakChecked &operator=(LeakChecked&&) = default;
private:
	struct Checker
	{
		Checker() = default;
		~Checker()
		{
			BOOST_ASSERT(counter == 0);
		}
		std::atomic_size_t counter{ 0 };
	};
	static Checker d;
};

template <typename T>
typename LeakChecked<T>::Checker LeakChecked<T>::d{};
#endif
