#include <yamail/resource_pool/handle.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <list>

namespace {

using namespace testing;

struct handle_test : Test {};

struct resource {};

struct idle {
    std::shared_ptr<resource> value;
};

struct pool {
    struct pool_impl {
        typedef resource value_type;
        typedef std::shared_ptr<resource> pointer;
        typedef std::list<idle>::iterator list_iterator;

        MOCK_METHOD1(waste, void (list_iterator));
    };

    typedef pool_impl::list_iterator list_iterator;
};

typedef yamail::resource_pool::handle<pool> resource_handle;

TEST(handle_test, construct_usable_should_be_not_unusable) {
    std::list<idle> resources({idle {}});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_FALSE(handle.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_than_source_should_be_unusable) {
    std::list<idle> resources({idle {}});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    const resource_handle dst = std::move(src);
    EXPECT_TRUE(src.unusable());
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_and_move_over_assign_than_source_should_be_unusable) {
    std::list<idle> resources({idle {}});
    const auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle src(pool_impl, &resource_handle::waste, resources.begin());
    resource_handle dst(pool_impl, &resource_handle::waste, resources.end());
    dst = std::move(src);
    EXPECT_TRUE(src.unusable());
    EXPECT_FALSE(dst.unusable());
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
}

TEST(handle_test, construct_usable_than_get_should_return_value) {
    const auto res = std::make_shared<resource>();
    std::list<idle> resources({idle {res}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(&handle.get(), res.get());
}

TEST(handle_test, construct_usable_than_get_const_should_return_value) {
    const auto res = std::make_shared<resource>();
    std::list<idle> resources({idle {res}});
    auto pool_impl = std::make_shared<pool::pool_impl>();
    const resource_handle handle(pool_impl, &resource_handle::waste, resources.begin());
    EXPECT_EQ(&handle.get(), res.get());
}

}
