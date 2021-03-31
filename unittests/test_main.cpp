#define BOOST_TEST_MODULE lemon tests
#define BOOST_TEST_MAIN
#include <boost/log/core/core.hpp>
#include <boost/test/unit_test.hpp>

struct GlobalFixture
{
	GlobalFixture()
	{
		boost::log::core::get()->remove_all_sinks();
	}
};

BOOST_TEST_GLOBAL_FIXTURE(GlobalFixture);