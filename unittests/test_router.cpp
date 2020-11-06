#include "router.hpp"
#include "rh_manager.hpp"
#include "request_handler.hpp"
#include "options.hpp"
#include <boost/test/unit_test.hpp>

namespace
{
struct Handler1 : RequestHandler
{
	string_view get_name() const noexcept override
	{
		return "h1"sv;
	}
};

struct Handler2 : RequestHandler
{
	string_view get_name() const noexcept override
	{
		return "h2"sv;
	}
};

struct RouterFixture
{
	RouterFixture()
	{
		man.add(h1);
		man.add(h2);
		routes.clear();
	}

	const std::shared_ptr<RequestHandler> h1 = std::make_shared<Handler1>();
	const std::shared_ptr<RequestHandler> h2 = std::make_shared<Handler2>();
	RhManager man;
	decltype(Options::servers) servers = { {} };
	Options::RouteList& routes = servers.at(0).routes;
};
}

#define assert_yes(r, path, module) \
	do { \
		const auto h = r.resolve(path); \
		BOOST_TEST(h); \
		BOOST_TEST(h == module.get()); \
	} while(0)

#define  assert_no(r, path) \
	do { \
		BOOST_TEST(!r.resolve(path)); \
	} while(0)

BOOST_FIXTURE_TEST_SUITE(router_tests, RouterFixture)

BOOST_AUTO_TEST_CASE(test_non_existent_module)
{
	routes.push_back({ Options::Route::Equal{"path"}, "bad_name" });
	BOOST_CHECK_THROW(Router(man, routes), std::exception);
}

BOOST_AUTO_TEST_CASE(test_match_exact)
{
	routes.push_back({ Options::Route::Equal{"path1"}, "h1" });
	routes.push_back({ Options::Route::Equal{"path2"}, "h2" });
	const Router r{ man, routes };

	assert_yes(r, "path1", h1);
	assert_yes(r, "path2", h2);
	assert_no(r, "path3");
	assert_no(r, "ath1");
	assert_no(r, "path");
}

BOOST_AUTO_TEST_CASE(test_match_prefix)
{
	routes.push_back({ Options::Route::Prefix{"path1"}, "h1" });
	routes.push_back({ Options::Route::Prefix{"path2"}, "h2" });
	const Router r{ man, routes };

	assert_yes(r, "path1", h1);
	assert_yes(r, "path10", h1);
	assert_yes(r, "path2", h2);
	assert_yes(r, "path20", h2);
	assert_no(r, "path3");
	assert_no(r, "ath1");
	assert_no(r, "path");
}

BOOST_AUTO_TEST_CASE(test_match_regex)
{
	routes.push_back({ Options::Route::Regex{"path[0-9]+"}, "h1" });
	routes.push_back({ Options::Route::Regex{"v1|v2|[abc]"}, "h2" });
	const Router r{ man, routes };

	assert_yes(r, "path0", h1);
	assert_yes(r, "path9", h1);
	assert_yes(r, "path10", h1);
	assert_yes(r, "v1", h2);
	assert_yes(r, "v2", h2);
	assert_yes(r, "a", h2);
	assert_yes(r, "b", h2);
	assert_yes(r, "c", h2);
	assert_no(r, "pathA");
	assert_no(r, "path1a");
	assert_no(r, "path[");
	assert_no(r, "ath1");
	assert_no(r, "path");
	assert_no(r, "v3");
	assert_no(r, "ab");
}

BOOST_AUTO_TEST_SUITE_END()