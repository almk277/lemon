#pragma once
#include "arena.hpp"
#include <boost/intrusive/list.hpp>

class logger;
struct alloc_tag_t;

class arena_imp : public arena
{
public:
	explicit arena_imp(logger &lg) noexcept;
	~arena_imp();

	void *aligned_alloc(size_t alignment, size_t size);

	void log_alloc(size_t size, const char *msg) const;
	void log_free(size_t size, const char *msg) const;

	size_t n_blocks_allocated() const noexcept;
	size_t n_bytes_allocated() const noexcept;

private:
	struct alignas(std::max_align_t) block : boost::intrusive::list_base_hook<>
	{
		explicit block(size_t) {}
		block(const block&) = delete;
		block(block&&) = delete;

		void *operator new(size_t size) = delete;
		void *operator new(size_t size, alloc_tag_t, size_t space);
		void operator delete(void *p);
		void operator delete(void *p, alloc_tag_t, size_t);
		void *free_space();
	};

	struct stateful_block : block
	{
		explicit stateful_block(size_t size);

		void *alloc(size_t size) noexcept;
		void *free_space();

		void *current;
		size_t space;
	};

	void *alloc_as_separate_block(size_t size);
	void *alloc_from_block(stateful_block &b, size_t size);
	void *alloc_in_new_block(size_t size);
	void *alloc_compact(size_t alignment, size_t size);

	logger &lg;
	size_t n_bytes = 0;

	boost::intrusive::list<stateful_block> avail_blocks;
	boost::intrusive::list<stateful_block> unavail_blocks;
	boost::intrusive::list<block> separate_blocks;
};
