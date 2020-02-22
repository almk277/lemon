#include "arena.hpp"
#include "arena_imp.hpp"
#include "logger.hpp"
#include <boost/assert.hpp>
#include <boost/align/aligned_alloc.hpp>
#include <algorithm>
#include <memory>
#include <new>

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

namespace
{
using std::size_t;

constexpr size_t BLOCK_SIZE = 4 * 1024; //TODO adjust to fit block here
constexpr size_t BLOCK_ALIGN_WANTED = 16;
constexpr size_t BLOCK_ALIGN = std::max(BLOCK_ALIGN_WANTED, alignof(std::max_align_t));
constexpr size_t MIN_BLOCK_USEFUL_SIZE = 32;
constexpr size_t MAX_ALLOC_COMPACT_SIZE = BLOCK_SIZE - MIN_BLOCK_USEFUL_SIZE;

constexpr bool is_power_of_two(size_t n) { return (n & (n - 1)) == 0; }

static_assert(is_power_of_two(BLOCK_SIZE));
static_assert(is_power_of_two(BLOCK_ALIGN_WANTED));
static_assert(is_power_of_two(BLOCK_ALIGN));
static_assert(MAX_ALLOC_COMPACT_SIZE < BLOCK_SIZE);

void *sys_alloc(size_t size)
{
	auto p = boost::alignment::aligned_alloc(BLOCK_ALIGN, size);
	if (BOOST_UNLIKELY(!p))
		throw std::bad_alloc{};
	return p;
}

void sys_free(void *p) noexcept
{
	boost::alignment::aligned_free(p);
}
}

struct alloc_tag_t {};
constexpr alloc_tag_t alloc_tag{};


void *arena_imp::block::operator new(size_t size, alloc_tag_t, size_t space)
{
	return sys_alloc(size + space);
}

void arena_imp::block::operator delete(void *p)
{
	sys_free(p);
}

void arena_imp::block::operator delete(void *p, alloc_tag_t, size_t)
{
	operator delete(p);
}

void *arena_imp::block::free_space() { return this + 1; }


arena_imp::stateful_block::stateful_block(size_t size):
	block{size},
	current{ free_space() },
	space{ size }
{}

void *arena_imp::stateful_block::alloc(size_t size) noexcept
{
	BOOST_ASSERT(size <= space);
	auto p = current;
	current = static_cast<std::byte*>(current) + size;
	space -= size;
	return p;
}

void *arena_imp::stateful_block::free_space() { return this + 1; }


template <typename Block, typename BlockList>
Block *make(size_t size, BlockList &where, size_t &n_bytes)
{
	auto b = new (alloc_tag, size) Block{ size };
	where.push_front(*b);
	n_bytes += size + sizeof(Block);
	return b;
}

arena_imp::arena_imp(logger &lg) noexcept:
	lg{lg}
{
	lg.debug("arena: creating arena");
}

arena_imp::~arena_imp()
{
	lg.debug("arena: destroying arena with ", n_blocks_allocated(),
		" blocks and ", n_bytes_allocated(), " bytes");

	separate_blocks.clear_and_dispose(std::default_delete<block>{});
	unavail_blocks.clear_and_dispose(std::default_delete<stateful_block>{});
	avail_blocks.clear_and_dispose(std::default_delete<stateful_block>{});
}

void *arena_imp::alloc_as_separate_block(size_t size)
{
	lg.debug("arena: allocating separate block of ", size, " bytes");
	auto b = make<block>(size, separate_blocks, n_bytes);
	return b->free_space();
}

void *arena_imp::alloc_from_block(stateful_block &b, size_t size)
{
	auto p = b.alloc(size);
	if (BOOST_UNLIKELY(b.space < MIN_BLOCK_USEFUL_SIZE)) {
		avail_blocks.erase(avail_blocks.iterator_to(b));
		unavail_blocks.push_back(b);
	}
	return p;
}

void *arena_imp::alloc_in_new_block(size_t size)
{
	lg.debug("arena: allocating block of ", size, " bytes");
	auto b = make<stateful_block>(BLOCK_SIZE, avail_blocks, n_bytes);
	return alloc_from_block(*b, size);
}

void *arena_imp::alloc_compact(size_t alignment, size_t size)
{
	for (auto &b : avail_blocks)
		if (std::align(alignment, size, b.current, b.space))
			return alloc_from_block(b, size);

	return alloc_in_new_block(size);
}

void *arena_imp::aligned_alloc(size_t alignment, size_t size)
{
	BOOST_ASSERT(is_power_of_two(alignment));
	if (size <= MAX_ALLOC_COMPACT_SIZE)
		return alloc_compact(alignment, size);
	return alloc_as_separate_block(size);
}

void arena_imp::log_alloc(size_t size, const char *msg) const
{
	lg.trace(msg, " allocate ", size);
}

void arena_imp::log_free(size_t size, const char *msg) const
{
	lg.trace(msg, " free ", size);
}

auto arena_imp::n_blocks_allocated() const noexcept -> size_t
{
	return avail_blocks.size()
		+ unavail_blocks.size()
		+ separate_blocks.size();
}

auto arena_imp::n_bytes_allocated() const noexcept -> size_t
{
	return n_bytes;
}

inline arena_imp *impl(arena *a)
{
	return static_cast<arena_imp*>(a);
}

inline const arena_imp *impl(const arena *a)
{
	return static_cast<const arena_imp*>(a);
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

auto arena::n_blocks_allocated() const noexcept -> size_t
{
	return impl(this)->n_blocks_allocated();
}

auto arena::n_bytes_allocated() const noexcept -> size_t
{
	return impl(this)->n_bytes_allocated();
}
