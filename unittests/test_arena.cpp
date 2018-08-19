// Effective with Address Sanitizer

#include "arena_imp.hpp"
#include "stub_logger.hpp"
#include <boost/test/unit_test.hpp>
#include <boost/align/is_aligned.hpp> 
#include <boost/mpl/list.hpp>
#include <memory>
#include <cstring>
#include <cstddef>

using std::size_t;

const size_t n_arenas = 8;
const size_t sizes[] = { 1, 10, 100, 1'000, 10'000, 1'000, 100, 10, 1 };
const size_t max_alignment = alignof(std::max_align_t);

namespace {
	class buffer
	{
		void *data;
		size_t size;
	public:
		buffer(void *data, size_t size) : data{ data }, size{ size } { test(); }
		void test() const { std::memset(data, 0, size); }
	};

	using buffer_list = std::vector<buffer>;

	struct big_array { int data[2048]; };

	struct arena_fixture
	{
		arena_imp a{ slg };
	};
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(arena::allocator<char>)

BOOST_FIXTURE_TEST_SUITE(arena_tests, arena_fixture)

BOOST_AUTO_TEST_CASE(test_aligned_alloc)
{
	std::vector<std::pair<std::unique_ptr<arena_imp>, buffer_list>> arenas;
	for (size_t i = 0; i < n_arenas; ++i)
		arenas.emplace_back(std::make_unique<arena_imp>(slg), buffer_list{});

	for (auto &a : arenas)
		for (auto size: sizes)
			for (size_t align = 1; align <= max_alignment; align *= 2)
			{
				const auto p = a.first->aligned_alloc(align, size);
				a.second.emplace_back(p, size);
				BOOST_TEST(boost::alignment::is_aligned(p, align));
			}

	arenas.pop_back();
	arenas.erase(arenas.begin() + arenas.size() / 2);
	arenas.erase(arenas.begin());

	for (auto &a : arenas)
		for (auto &b : a.second)
			b.test();
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

using alloc_types = boost::mpl::list<
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
	std::list<int, arena::allocator<int>> l {
		{ 1, 2, 3, 4 },
		a.make_allocator<int>()
	};
}

BOOST_AUTO_TEST_CASE(test_allocator_compare)
{
	arena_imp a2{ slg };
	BOOST_TEST(a.make_allocator<char>() == a.make_allocator<char>());
	BOOST_TEST(a.make_allocator<char>() == arena::allocator<char>(a));
	BOOST_TEST(a.make_allocator<char>() != a2.make_allocator<char>());
}

BOOST_AUTO_TEST_SUITE_END()