#ifndef YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
#define YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <list>

#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <boost/chrono/system_clocks.hpp>

namespace tests {

using namespace testing;
using namespace yamail::resource_pool;

using boost::ref;
using boost::make_shared;
using boost::chrono::seconds;

struct resource {};
struct request {};

typedef boost::shared_ptr<resource> resource_ptr;
typedef boost::chrono::steady_clock::time_point time_point;
typedef boost::chrono::steady_clock::duration time_duration;

struct mocked_io_service {
    MOCK_METHOD1(post, void (boost::function<void ()>));
};

struct mocked_timer {
    mocked_timer() {}
    mocked_timer(mocked_io_service&) {}

    MOCK_CONST_METHOD0(expires_at, time_point ());
    MOCK_CONST_METHOD1(expires_at, void (const time_point&));
    MOCK_CONST_METHOD1(async_wait, void (boost::function<void (boost::system::error_code)>));
};

} // namespace tests

#endif // YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
