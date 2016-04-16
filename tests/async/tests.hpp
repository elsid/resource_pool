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

class mocked_on_catch_handler_exception {
public:
    struct impl_type {
        MOCK_CONST_METHOD1(call, void (const boost::system::error_code&));
    };

    mocked_on_catch_handler_exception() : _impl(new impl_type) {}

    const impl_type& impl() const { return *_impl; }

    void operator ()(const boost::system::error_code& ec) const {
        impl().call(ec);
    }

private:
    boost::shared_ptr<impl_type> _impl;
};

} // namespace tests

#endif // YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
