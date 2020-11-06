// Effective with Address Sanitizer

#include "arena_imp.hpp"
#include "stub_logger.hpp"
#include <boost/asio/buffer.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/align/is_aligned.hpp> 
#include <memory>
#include <cstring>
#include <cstddef>
#include <vector>
#include <tuple>
#include <utility>

using std::size_t;

namespace
{
const size_t n_arenas = 8;
const size_t sizes[] = { 1, 10, 100, 1'000, 10'000, 1'000, 100, 10, 1 };
const size_t max_alignment = alignof(std::max_align_t);

using buffer = boost::asio::mutable_buffer;
using buffer_list = std::vector<buffer>;
struct big_array { int data[2048]; };

struct ArenaFixture
{
	ArenaImp a{ slg };
};

void test1(const buffer& b) { std::memset(b.data(), 0, b.size()); }
void test(const buffer_list& list) { boost::range::for_each(list, test1); }
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(Arena::Allocator<char>)

BOOST_FIXTURE_TEST_SUITE(arena_tests, ArenaFixture)

BOOST_AUTO_TEST_CASE(test_aligned_alloc)
{
	std::vector<std::pair<std::unique_ptr<ArenaImp>, buffer_list>> arenas;
	for (size_t i = 0; i < n_arenas; ++i)
		arenas.emplace_back(std::make_unique<ArenaImp>(slg), buffer_list{});

	for (auto& [a, buffers] : arenas)
		for (auto size: sizes)
			for (size_t align = 1; align <= max_alignment; align *= 2)
			{
				const auto p = a->aligned_alloc(align, size);
				buffers.emplace_back(p, size);
				BOOST_TEST(boost::alignment::is_aligned(p, align));
			}

	arenas.pop_back();
	arenas.erase(arenas.begin() + arenas.size() / 2);
	arenas.erase(arenas.begin());

	for (auto& a : arenas)
		test(a.second);
}

BOOST_AUTO_TEST_CASE(test_alloc)
{
	size_t n_bytes = 0;
	for (auto size : sizes)
	{
		const auto p = a.alloc(size);
		n_bytes += size;
		BOOST_TEST(boost::alignment::is_aligned(p, max_alignment));
		BOOST_TEST(a.n_bytes_allocated() >= n_bytes);
	}
}

using alloc_types = std::tuple<
	char, short, int, long, long long,
	float, double, long double,
	big_array
>;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_alloc_type, T, alloc_types)
{
	auto p = a.alloc<T>();
	BOOST_TEST(boost::alignment::is_aligned(p, alignof(T)));
	BOOST_TEST(a.n_bytes_allocated() >= sizeof(T));
}

BOOST_AUTO_TEST_CASE(test_allocator)
{
	std::list l {
		{ 1, 2, 3, 4 },
		a.make_allocator<int>()
	};
}

BOOST_AUTO_TEST_CASE(test_allocator_compare)
{
	ArenaImp a2{ slg };
	BOOST_TEST(a.make_allocator<char>() == a.make_allocator<char>());
	BOOST_TEST(a.make_allocator<char>() == Arena::Allocator<char>(a));
	BOOST_TEST(a.make_allocator<char>() != a2.make_allocator<char>());
}

BOOST_AUTO_TEST_SUITE_END()