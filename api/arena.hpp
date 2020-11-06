#pragma once
#include <cstddef>

class Arena
{
public:
	using size_t = std::size_t;

	Arena(const Arena&) = delete;
	Arena(Arena&&) = delete;
	Arena &operator=(const Arena&) = delete;
	Arena &operator=(Arena&&) = delete;

	template <typename T>
	[[nodiscard]] void *alloc(const char *msg = "");

	[[nodiscard]] void *alloc(size_t size, const char *msg = "");
	void free(void *ptr, size_t size, const char *msg = "") noexcept;

	[[nodiscard]] void *aligned_alloc(size_t alignment, size_t size, const char *msg = "");

	size_t n_blocks_allocated() const noexcept;
	size_t n_bytes_allocated() const noexcept;
	
	template <typename T> class Allocator;

	template <typename T>
	constexpr Allocator<T> make_allocator(const char *name = "") noexcept
	{ return Allocator<T>{*this, name}; }

protected:
	Arena() = default;
	~Arena() = default;

private:
	void *aligned_alloc_imp(size_t alignment, size_t size);

	void log_alloc(size_t size, const char *msg) const;
	void log_free(size_t size, const char *msg) const;
};

template <typename T>
void *Arena::alloc(const char *msg)
{
	return aligned_alloc(alignof(T), sizeof(T), msg);
}

inline void *Arena::alloc(size_t size, const char *msg)
{
	return aligned_alloc(alignof(std::max_align_t), size, msg);
}

inline void Arena::free(void*, size_t s, const char *msg) noexcept
{
	log_free(s, msg);
}

inline void *Arena::aligned_alloc(size_t alignment, size_t size, const char *msg)
{
	log_alloc(size, msg);
	return aligned_alloc_imp(alignment, size);
}


// Implements std::Allocator
template <typename T>
class Arena::Allocator
{
public:
	using value_type = T;

	explicit constexpr Allocator(Arena &a, const char *name = "") noexcept:
		a{ a }, name{ name } {}
	template <typename U>
	explicit Allocator(const Allocator<U> &rhs) noexcept :
		a{ rhs.a }, name{ rhs.name } {}
	template <typename U>
	explicit Allocator(Allocator<U> &&rhs) noexcept :
		a{ rhs.a }, name{ rhs.name } {}
	~Allocator() = default;

	T *allocate(std::size_t n)
	{
		return static_cast<T*>(a.aligned_alloc(alignof(T), n * sizeof(T), name));
	}
	void deallocate(T *p, std::size_t n) noexcept
	{
		a.free(p, n * sizeof(T), name);
	}

	template <typename U>
	struct rebind
	{
		using other = Allocator<U>;
	};

	friend bool operator==(const Allocator<T> &a1, const Allocator<T> &a2) noexcept
	{
		return &a1.a == &a2.a;
	}
	friend bool operator!=(const Allocator<T> &a1, const Allocator<T> &a2) noexcept
	{
		return &a1.a != &a2.a;
	}

private:
	Arena &a;
	const char *const name;
	template <typename> friend class Allocator;
};