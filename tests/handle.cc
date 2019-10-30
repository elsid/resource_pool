#include <yamail/resource_pool/handle.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/optional.hpp>

#include <list>

namespace {

using namespace testing;

struct handle_test : Test {};

struct resource {
    int value = 0;

    resource(int value = 0) : value(value) {}
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource &) = delete;
    resource& operator =(resource &&) = default;
};

struct idle {
    boost::optional<resource> value;
};

struct pool {
    struct pool_impl {
        using value_type = resource;
        using list_iterator = std::list<idle>::iterator;

        MOCK_METHOD1(waste, void (list_iterator));
    };

    using list_iterator = pool_impl::list_iterator;
};

using resource_handle = yamail::resource_pool::handle<pool::pool_impl>;

TEST(handle_test, construct_usable_should_be_not_unusable) {
    std::list<idle> resources;
    resources.emplace_back(idle());
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_FALSE(handle.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_then_destination_should_contain_value) {
    std::list<idle> resources;
    resources.emplace_back(idle());
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    const resource_handle dst = std::move(src);
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_over_assign_then_destination_should_contain_value) {
    std::list<idle> resources;
    resources.emplace_back(idle());
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    resource_handle dst(pool_impl, &resource_handle::waste, resources.end());
    dst = std::move(src);
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_then_get_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource(42)});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_then_get_const_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(idle {resource(42)});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());

}

}
