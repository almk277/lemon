#pragma once
#include <atomic>
#include <boost/assert.hpp>

#ifdef BOOST_ASSERT_IS_VOID

template <typename> class leak_checked {};

#else

template <typename T>
class leak_checked
{
protected:
	leak_checked() { ++d.counter; }
	leak_checked(const leak_checked&) { ++d.counter; }
	leak_checked(leak_checked&&) noexcept { ++d.counter; }
	~leak_checked() { --d.counter; }
	leak_checked &operator=(const leak_checked&) = default;
	leak_checked &operator=(leak_checked&&) = default;
private:
	struct checker
	{
		checker() = default;
		~checker()
		{
			BOOST_ASSERT(counter == 0);
		}
		std::atomic_size_t counter{ 0 };
	};
	static checker d;
};

template <typename T>
typename leak_checked<T>::checker leak_checked<T>::d{};
#endif
