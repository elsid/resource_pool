#ifndef YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
#define YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP

#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <list>

namespace tests {

using namespace testing;
using namespace yamail::resource_pool;

struct resource {
    resource() = default;
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource&) = delete;
    resource& operator =(resource&&) = default;
};

struct request {
    int value;
};

struct mocked_io_service {
    MOCK_METHOD1(post, void (std::function<void ()>));
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
    std::shared_ptr<impl_type> _impl;
};

} // namespace tests

#endif // YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
