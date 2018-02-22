#include "arena.hpp"
#include "logger.hpp"
#include <boost/assert.hpp>
#include <boost/static_assert.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/align/aligned_alloc.hpp>
#include <algorithm>
#include <memory>
#include <new>

using std::size_t;

constexpr size_t BLOCK_SIZE = 4 * 1024;
constexpr size_t BLOCK_ALIGN_WANTED = 16;
constexpr size_t BLOCK_ALIGN = std::max(BLOCK_ALIGN_WANTED, alignof(std::max_align_t));
constexpr size_t MIN_BLOCK_USEFUL_SIZE = 32;
constexpr size_t MAX_ALLOC_COMPACT_SIZE = BLOCK_SIZE - MIN_BLOCK_USEFUL_SIZE;

constexpr bool is_power_of_two(size_t n) { return (n & (n - 1)) == 0; }

BOOST_STATIC_ASSERT(is_power_of_two(BLOCK_SIZE));
BOOST_STATIC_ASSERT(is_power_of_two(BLOCK_ALIGN_WANTED));
BOOST_STATIC_ASSERT(MAX_ALLOC_COMPACT_SIZE < BLOCK_SIZE);

#if 0
#include <cstdio>
#include <cstdlib>
void *operator new(size_t size)
{
	auto p = std::malloc(size);
	std::printf("NEW %zu -> %p\n", size, p);
	return p;
}

void operator delete(void *p)
{
	std::printf("DELETE %p\n", p);
	std::free(p);
}
#endif

static void *sys_alloc(size_t size)
{
	auto p = boost::alignment::aligned_alloc(BLOCK_ALIGN, size);
	if (BOOST_UNLIKELY(!p))
		throw std::bad_alloc{};
	return p;
}

static void sys_free(void *p) noexcept
{
	boost::alignment::aligned_free(p);
}

struct alloc_tag_t {};
constexpr alloc_tag_t alloc_tag{};

//TODO anonymous namespace?
struct alignas(std::max_align_t) block : boost::intrusive::list_base_hook<>
{
	block(size_t) {}
	block(const block&) = delete;
	block(block&&) = delete;

	void *operator new(size_t size, alloc_tag_t, size_t space)
	{
		return sys_alloc(size + space);
	}
	void operator delete(void *p)
	{
		sys_free(p);
	}
	void operator delete(void *p, alloc_tag_t, size_t)
	{
		operator delete(p);
	}
	void *free_space() { return this + 1; }
};

struct stateful_block : block
{
	explicit stateful_block(size_t size):
		block{size},
		current{ free_space() },
		space{ size }
	{}

	void *alloc(size_t size) noexcept
	{
		BOOST_ASSERT(size <= space);
		auto p = current;
		current = static_cast<char*>(current) + size;
		space -= size;
		return p;
	}

	void *free_space() { return this + 1; }

	void *current;
	size_t space;
};

class arena_imp: public arena
{
	boost::intrusive::list<stateful_block> avail_blocks;
	boost::intrusive::list<stateful_block> unavail_blocks;
	boost::intrusive::list<block> separate_blocks;

	logger *lg;
	size_t n_bytes = 0;
	
	explicit arena_imp(logger *lg) noexcept:
		lg{lg} {}
	~arena_imp() = default;

	template <typename Block, typename BlockList>
	Block *make(size_t size, BlockList &where)
	{
		lg->debug("arena: allocating block of ", size);
		auto b = new (alloc_tag, size) Block{size};
		where.push_front(*b);
		n_bytes += size + sizeof(Block);
		return b;
	}

	void *alloc_as_separate_block(size_t size)
	{
		auto b = make<block>(size, separate_blocks);
		return b->free_space();
	}

	void *alloc_from_block(stateful_block &b, size_t size)
	{
		auto p = b.alloc(size);
		if (BOOST_UNLIKELY(b.space < MIN_BLOCK_USEFUL_SIZE)) {
			avail_blocks.erase(avail_blocks.iterator_to(b));
			unavail_blocks.push_back(b);
		}
		return p;
	}

	void *alloc_in_new_block(size_t size)
	{
		auto b = make<stateful_block>(BLOCK_SIZE, avail_blocks);
		return alloc_from_block(*b, size);
	}

	void *alloc_compact(size_t alignment, size_t size)
	{
		for (auto &b : avail_blocks)
			if (std::align(alignment, size, b.current, b.space))
				return alloc_from_block(b, size);

		return alloc_in_new_block(size);
	}

public:
	static arena_imp *make(logger *lg)
	{
		BOOST_ASSERT(lg);
		lg->debug("arena: creating arena, allocating block of ", BLOCK_SIZE);
		auto b = new (alloc_tag, BLOCK_SIZE) stateful_block{ BLOCK_SIZE };
		BOOST_STATIC_ASSERT(sizeof(arena_imp) < MAX_ALLOC_COMPACT_SIZE);
		auto a = new (b->alloc(sizeof(arena_imp))) arena_imp{ lg };
		a->avail_blocks.push_front(*b);
		a->n_bytes += BLOCK_SIZE + sizeof(stateful_block);
		return a;
	}
	static void remove(arena_imp *a) noexcept
	{
		a->lg->debug("arena: destroying arena with ", a->n_blocks_allocated(),
			" blocks and ", a->n_bytes_allocated(), " bytes");

		a->separate_blocks.clear_and_dispose(std::default_delete<block>{});
		// arena itself is placed inside it's blocks, so
		// we can't free all blocks until arena dtor is completed
		auto unavail = std::move(a->unavail_blocks);
		auto avail = std::move(a->avail_blocks);

		a->~arena_imp();

		unavail.clear_and_dispose(std::default_delete<stateful_block>{});
		avail.clear_and_dispose(std::default_delete<stateful_block>{});
	}

	void *aligned_alloc(size_t alignment, size_t size)
	{
		BOOST_ASSERT(is_power_of_two(alignment));
		if (size <= MAX_ALLOC_COMPACT_SIZE)
			return alloc_compact(alignment, size);
		return alloc_as_separate_block(size);
	}
	void log_alloc(size_t size, const char *msg) const
	{
		lg->trace(msg, " allocate ", size);
	}

	void log_free(size_t size, const char *msg) const
	{
		lg->trace(msg, " free ", size);
	}
	void set_logger(logger *lg) noexcept
	{
	  BOOST_ASSERT(lg);
	  this->lg = lg;
	}
	size_t n_blocks_allocated() const noexcept
	{
		return avail_blocks.size()
			+ unavail_blocks.size()
			+ separate_blocks.size();
	}
	size_t n_bytes_allocated() const noexcept
	{
		return n_bytes;
	}
};

inline arena_imp *impl(arena *a)
{
	return static_cast<arena_imp*>(a);
}

inline const arena_imp *impl(const arena *a)
{
	return static_cast<const arena_imp*>(a);
}

arena *arena::make(logger *lg)
{
	return arena_imp::make(lg);
}

void arena::remove(arena *a) noexcept
{
	arena_imp::remove(impl(a));
}

void *arena::aligned_alloc_imp(size_t alignment, size_t size)
{
	return impl(this)->aligned_alloc(alignment, size);
}

void arena::log_alloc(size_t size, const char *msg) const
{
	impl(this)->log_alloc(size, msg);
}

void arena::log_free(size_t size, const char *msg) const
{
	impl(this)->log_free(size, msg);
}

size_t arena::n_blocks_allocated() const noexcept
{
	return impl(this)->n_blocks_allocated();
}

size_t arena::n_bytes_allocated() const noexcept
{
	return impl(this)->n_bytes_allocated();
}

void arena_set_logger(arena *a, logger *lg) noexcept
{
	impl(a)->set_logger(lg);
}