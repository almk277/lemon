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
void* operator new(size_t size)
{
	auto p = std::malloc(size);
	std::printf("NEW %zu -> %p\n", size, p);
	return p;
}

void operator delete(void* p)
{
	std::printf("DELETE %p\n", p);
	std::free(p);
}
#endif

namespace
{
using std::size_t;

constexpr size_t block_size = 4 * 1024; //TODO adjust to fit block here
constexpr size_t block_align_wanted = 16;
constexpr size_t block_align = std::max(block_align_wanted, alignof(std::max_align_t));
constexpr size_t min_block_useful_size = 32;
constexpr size_t max_alloc_compact_size = block_size - min_block_useful_size;

constexpr bool is_power_of_two(size_t n) { return (n & (n - 1)) == 0; }

static_assert(is_power_of_two(block_size));
static_assert(is_power_of_two(block_align_wanted));
static_assert(is_power_of_two(block_align));
static_assert(max_alloc_compact_size < block_size);

void* sys_alloc(size_t size)
{
	auto p = boost::alignment::aligned_alloc(block_align, size);
	if (BOOST_UNLIKELY(!p))
		throw std::bad_alloc{};
	return p;
}

void sys_free(void* p) noexcept
{
	boost::alignment::aligned_free(p);
}
}

struct AllocTag {};
constexpr AllocTag alloc_tag{};


void* ArenaImp::Block::operator new(size_t size, AllocTag, size_t space)
{
	return sys_alloc(size + space);
}

void ArenaImp::Block::operator delete(void* p)
{
	sys_free(p);
}

void ArenaImp::Block::operator delete(void* p, AllocTag, size_t)
{
	operator delete(p);
}

void* ArenaImp::Block::free_space() { return this + 1; }


ArenaImp::StatefulBlock::StatefulBlock(size_t size):
	Block{size},
	current{ free_space() },
	space{ size }
{}

void* ArenaImp::StatefulBlock::alloc(size_t size) noexcept
{
	BOOST_ASSERT(size <= space);
	auto p = current;
	current = static_cast<std::byte*>(current) + size;
	space -= size;
	return p;
}

void* ArenaImp::StatefulBlock::free_space() { return this + 1; }


template <typename Block, typename BlockList>
Block* make(size_t size, BlockList& where, size_t& n_bytes)
{
	auto b = new (alloc_tag, size) Block{ size };
	where.push_front(*b);
	n_bytes += size + sizeof(Block);
	return b;
}

ArenaImp::ArenaImp(Logger& lg) noexcept:
	lg{lg}
{
	lg.debug("arena: creating arena");
}

ArenaImp::~ArenaImp()
{
	lg.debug("arena: destroying arena with ", n_blocks_allocated(),
		" blocks and ", n_bytes_allocated(), " bytes");

	separate_blocks.clear_and_dispose(std::default_delete<Block>{});
	unavail_blocks.clear_and_dispose(std::default_delete<StatefulBlock>{});
	avail_blocks.clear_and_dispose(std::default_delete<StatefulBlock>{});
}

void* ArenaImp::alloc_as_separate_block(size_t size)
{
	lg.debug("arena: allocating separate block of ", size, " bytes");
	auto b = make<Block>(size, separate_blocks, n_bytes);
	return b->free_space();
}

void* ArenaImp::alloc_from_block(StatefulBlock& b, size_t size)
{
	auto p = b.alloc(size);
	if (BOOST_UNLIKELY(b.space < min_block_useful_size)) {
		avail_blocks.erase(avail_blocks.iterator_to(b));
		unavail_blocks.push_back(b);
	}
	return p;
}

void* ArenaImp::alloc_in_new_block(size_t size)
{
	lg.debug("arena: allocating block of ", size, " bytes");
	auto b = make<StatefulBlock>(block_size, avail_blocks, n_bytes);
	return alloc_from_block(*b, size);
}

void* ArenaImp::alloc_compact(size_t alignment, size_t size)
{
	for (auto& b : avail_blocks)
		if (std::align(alignment, size, b.current, b.space))
			return alloc_from_block(b, size);

	return alloc_in_new_block(size);
}

void* ArenaImp::aligned_alloc(size_t alignment, size_t size)
{
	BOOST_ASSERT(is_power_of_two(alignment));
	if (size <= max_alloc_compact_size)
		return alloc_compact(alignment, size);
	return alloc_as_separate_block(size);
}

void ArenaImp::log_alloc(size_t size, const char* msg) const
{
	lg.trace(msg, " allocate ", size);
}

void ArenaImp::log_free(size_t size, const char* msg) const
{
	lg.trace(msg, " free ", size);
}

auto ArenaImp::n_blocks_allocated() const noexcept -> size_t
{
	return avail_blocks.size()
		+ unavail_blocks.size()
		+ separate_blocks.size();
}

auto ArenaImp::n_bytes_allocated() const noexcept -> size_t
{
	return n_bytes;
}

inline ArenaImp* impl(Arena* a)
{
	return static_cast<ArenaImp*>(a);
}

inline const ArenaImp* impl(const Arena* a)
{
	return static_cast<const ArenaImp*>(a);
}

void* Arena::aligned_alloc_imp(size_t alignment, size_t size)
{
	return impl(this)->aligned_alloc(alignment, size);
}

void Arena::log_alloc(size_t size, const char* msg) const
{
	impl(this)->log_alloc(size, msg);
}

void Arena::log_free(size_t size, const char* msg) const
{
	impl(this)->log_free(size, msg);
}

auto Arena::n_blocks_allocated() const noexcept -> size_t
{
	return impl(this)->n_blocks_allocated();
}

auto Arena::n_bytes_allocated() const noexcept -> size_t
{
	return impl(this)->n_bytes_allocated();
}