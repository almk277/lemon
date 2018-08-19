#pragma once

#include <cstddef>

class arena
{
public:
	using size_t = std::size_t;

	arena(const arena&) = delete;
	arena(arena&&) = delete;
	arena &operator=(const arena&) = delete;
	arena &operator=(arena&&) = delete;

	template <typename T>
	void *alloc(const char *msg = "");

	void *alloc(size_t size, const char *msg = "");
	void free(void *ptr, size_t size, const char *msg = "") noexcept;

	void *aligned_alloc(size_t alignment, size_t size, const char *msg = "");

	size_t n_blocks_allocated() const noexcept;
	size_t n_bytes_allocated() const noexcept;
	
	template <typename T> class allocator;

	template <typename T>
	allocator<T> make_allocator(const char *name = "") noexcept
	{ return allocator<T>{*this, name}; }

protected:
	arena() = default;
	~arena() = default;

private:
	void *aligned_alloc_imp(size_t alignment, size_t size);

	void log_alloc(size_t size, const char *msg) const;
	void log_free(size_t size, const char *msg) const;
};

template <typename T>
void *arena::alloc(const char *msg)
{
	return aligned_alloc(alignof(T), sizeof(T), msg);
}

inline void *arena::alloc(size_t size, const char *msg)
{
	return aligned_alloc(alignof(std::max_align_t), size, msg);
}

inline void arena::free(void*, size_t s, const char *msg) noexcept
{
	log_free(s, msg);
}

inline void *arena::aligned_alloc(size_t alignment, size_t size, const char *msg)
{
	log_alloc(size, msg);
	return aligned_alloc_imp(alignment, size);
}


// Implements std::Allocator
template <typename T>
class arena::allocator
{
public:
	using value_type = T;

	explicit allocator(arena &a, const char *name = "") noexcept:
	a{ a }, name{ name } {}
	template <typename U>
	explicit allocator(const allocator<U> &rhs) noexcept :
		a{ rhs.a }, name{ rhs.name } {}
	template <typename U>
	explicit allocator(allocator<U> &&rhs) noexcept :
		a{ rhs.a }, name{ rhs.name } {}
	~allocator() = default;

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
		using other = allocator<U>;
	};

	friend bool operator==(const allocator<T> &a1, const allocator<T> &a2) noexcept
	{
		return &a1.a == &a2.a;
	}
	friend bool operator!=(const allocator<T> &a1, const allocator<T> &a2) noexcept
	{
		return &a1.a != &a2.a;
	}

private:
	arena &a;
	const char *const name;
	template <typename> friend class allocator;
};
