#include <yamail/resource_pool/sync/pool.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync;

struct resource {
    resource() = default;
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource&) = delete;
    resource& operator =(resource&&) = default;
};

struct mocked_pool_impl {
    using value_type = resource;
    using idle = yamail::resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = list::iterator;
    using get_result = std::pair<boost::system::error_code, list_iterator>;

    mocked_pool_impl(std::size_t, time_traits::duration) {}

    MOCK_CONST_METHOD0(capacity, std::size_t ());
    MOCK_CONST_METHOD0(size, std::size_t ());
    MOCK_CONST_METHOD0(available, std::size_t ());
    MOCK_CONST_METHOD0(used, std::size_t ());
    MOCK_CONST_METHOD0(stats, sync::stats ());
    MOCK_CONST_METHOD1(get, get_result (time_traits::duration));
    MOCK_CONST_METHOD1(recycle, void (list_iterator));
    MOCK_CONST_METHOD1(waste, void (list_iterator));
    MOCK_CONST_METHOD0(disable, void ());
};

using resource_pool = pool<resource, std::mutex, mocked_pool_impl>;

struct sync_resource_pool : Test {
    mocked_pool_impl::list resources;
    mocked_pool_impl::list_iterator resource_iterator;

    sync_resource_pool()
        : resources(1), resource_iterator(resources.begin()) {}
};

TEST_F(sync_resource_pool, create_without_mocks_should_succeed) {
    pool<resource>(1);
}

TEST_F(sync_resource_pool, call_capacity_should_call_impl_capacity) {
    const resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), capacity()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.capacity();
}

TEST_F(sync_resource_pool, call_size_should_call_impl_size) {
    const resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), size()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.size();
}

TEST_F(sync_resource_pool, call_available_should_call_impl_available) {
    const resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), available()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.available();
}

TEST_F(sync_resource_pool, call_used_should_call_impl_used) {
    const resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), used()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.used();
}

TEST_F(sync_resource_pool, call_stats_should_call_impl_stats) {
    const resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), stats()).WillOnce(Return(sync::stats {0, 0, 0}));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.stats();
}

TEST_F(sync_resource_pool, move_than_dtor_should_call_disable_only_for_destination) {
    resource_pool src(1);

    EXPECT_CALL(src.impl(), disable()).Times(0);

    const auto dst = std::move(src);

    EXPECT_CALL(dst.impl(), disable()).WillOnce(Return());
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_should_call_recycle) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    auto res = pool.get_auto_recycle();
    auto& ec = res.first;
    auto& handle = res.second;

    EXPECT_EQ(ec, boost::system::error_code());
    EXPECT_FALSE(handle.unusable());
    EXPECT_TRUE(handle.empty());
    EXPECT_NO_THROW(handle.reset(resource {}));
    EXPECT_NO_THROW(handle.get());
}

TEST_F(sync_resource_pool, get_auto_waste_handle_should_call_waste) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const auto res = pool.get_auto_waste();
    const auto& handle = res.second;

    EXPECT_FALSE(handle.unusable());
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    auto res = pool.get_auto_recycle();
    auto& handle = res.second;

    handle.recycle();

    EXPECT_TRUE(handle.unusable());
    EXPECT_THROW(handle.recycle(), error::unusable_handle);
    EXPECT_TRUE(handle.empty());
    EXPECT_THROW(handle.get(), error::empty_handle);
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_and_waste_should_call_waste_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    auto res = pool.get_auto_recycle();
    auto& handle = res.second;

    handle.waste();
}

TEST_F(sync_resource_pool, get_auto_waste_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    auto res = pool.get_auto_waste();
    auto& handle = res.second;

    handle.recycle();
}

TEST_F(sync_resource_pool, get_auto_waste_handle_and_waste_should_call_waste_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    auto res = pool.get_auto_waste();
    auto& handle = res.second;

    handle.waste();
}

}
