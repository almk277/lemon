#include "router.hpp"
#include "rh_manager.hpp"
#include "request_handler.hpp"
#include "parameters.hpp"
#include "options.hpp"
#include "stub_logger.hpp"
#include <boost/test/unit_test.hpp>

namespace {
	struct handler1 : request_handler
	{
		string_view get_name() const noexcept override
		{
			return "h1"_w;
		}
	};

	struct handler2 : request_handler
	{
		string_view get_name() const noexcept override
		{
			return "h2"_w;
		}
	};

	struct router_fixture
	{
		router_fixture()
		{
			man.add(h1);
			man.add(h2);
			opt.routes.clear();
		}

		const std::shared_ptr<request_handler> h1 = std::make_shared<handler1>();
		const std::shared_ptr<request_handler> h2 = std::make_shared<handler2>();
		rh_manager man;
		stub_logger lg;
		options opt{ parameters{0, nullptr, lg}, lg };
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

BOOST_FIXTURE_TEST_SUITE(router_tests, router_fixture)

BOOST_AUTO_TEST_CASE(test_non_existent_module)
{
	opt.routes.push_back({ options::route::equal{"path"}, "bad_name" });
	BOOST_CHECK_THROW(router(man, opt), std::exception);
}

BOOST_AUTO_TEST_CASE(test_match_exact)
{
	opt.routes.push_back({ options::route::equal{"path1"}, "h1" });
	opt.routes.push_back({ options::route::equal{"path2"}, "h2" });
	const router r{ man, opt };

	assert_yes(r, "path1", h1);
	assert_yes(r, "path2", h2);
	assert_no(r, "path3");
	assert_no(r, "ath1");
	assert_no(r, "path");
}

BOOST_AUTO_TEST_CASE(test_match_prefix)
{
	opt.routes.push_back({ options::route::prefix{"path1"}, "h1" });
	opt.routes.push_back({ options::route::prefix{"path2"}, "h2" });
	const router r{ man, opt };

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
	opt.routes.push_back({ options::route::regex{"path[0-9]+"}, "h1" });
	opt.routes.push_back({ options::route::regex{"v1|v2|[abc]"}, "h2" });
	const router r{ man, opt };

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