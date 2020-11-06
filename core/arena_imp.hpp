#pragma once
#include "arena.hpp"
#include <boost/intrusive/list.hpp>

class Logger;
struct AllocTag;

class ArenaImp : public Arena
{
public:
	explicit ArenaImp(Logger &lg) noexcept;
	~ArenaImp();

	void *aligned_alloc(size_t alignment, size_t size);

	void log_alloc(size_t size, const char *msg) const;
	void log_free(size_t size, const char *msg) const;

	size_t n_blocks_allocated() const noexcept;
	size_t n_bytes_allocated() const noexcept;

private:
	struct alignas(std::max_align_t) Block : boost::intrusive::list_base_hook<>
	{
		explicit Block(size_t) {}
		Block(const Block&) = delete;
		Block(Block&&) = delete;

		void *operator new(size_t size) = delete;
		void *operator new(size_t size, AllocTag, size_t space);
		void operator delete(void *p);
		void operator delete(void *p, AllocTag, size_t);
		void *free_space();
	};

	struct StatefulBlock : Block
	{
		explicit StatefulBlock(size_t size);

		void *alloc(size_t size) noexcept;
		void *free_space();

		void *current;
		size_t space;
	};

	void *alloc_as_separate_block(size_t size);
	void *alloc_from_block(StatefulBlock &b, size_t size);
	void *alloc_in_new_block(size_t size);
	void *alloc_compact(size_t alignment, size_t size);

	Logger &lg;
	size_t n_bytes = 0;

	boost::intrusive::list<StatefulBlock> avail_blocks;
	boost::intrusive::list<StatefulBlock> unavail_blocks;
	boost::intrusive::list<Block> separate_blocks;
};