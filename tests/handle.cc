#include <yamail/resource_pool/handle.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/optional.hpp>

#include <list>

namespace {

using namespace testing;

using yamail::resource_pool::time_traits;
using yamail::resource_pool::detail::pool_returns;

struct handle_test : Test {};

struct resource {
    int value = 0;

    resource(int value = 0) : value(value) {}
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource &) = delete;
    resource& operator =(resource &&) = default;
};

using list_iterator = yamail::resource_pool::detail::cell_iterator<resource>;

struct pool_impl_mock : pool_returns<resource> {
    MOCK_METHOD1(recycle, void (list_iterator));
    MOCK_METHOD1(waste, void (list_iterator));
};

using idle = yamail::resource_pool::detail::idle<resource>;
using resource_handle = yamail::resource_pool::handle<resource>;

TEST(handle_test, construct_usable_should_be_not_unusable) {
    std::list<idle> resources;
    resources.emplace_back();
    const auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_TRUE(handle.usable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_then_destination_should_contain_value) {
    std::list<idle> resources;
    resources.emplace_back();
    const auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    const resource_handle dst = std::move(src);
    EXPECT_TRUE(dst.usable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_over_assign_then_destination_should_contain_value) {
    std::list<idle> resources;
    resources.emplace_back();
    const auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    resource_handle dst;
    dst = std::move(src);
    EXPECT_TRUE(dst.usable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_then_get_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(resource(42), time_traits::time_point());
    auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_then_get_const_should_return_value) {
    std::list<idle> resources;
    resources.emplace_back(resource(42), time_traits::time_point());
    auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(42, handle->value);
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, move_to_usable_should_release_replaced_resource) {
    std::list<idle> resources(2);
    const auto pool_impl = std::make_shared<StrictMock<pool_impl_mock>>();
    const auto src_res = resources.begin();
    const auto dst_res = std::next(resources.begin());
    resource_handle src(pool_impl, &resource_handle::waste, src_res);
    resource_handle dst(pool_impl, &resource_handle::waste, dst_res);

    EXPECT_CALL(*pool_impl, waste(dst_res)).WillOnce(Return());
    dst = std::move(src);

    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(src_res)).WillOnce(Return());
}

}
