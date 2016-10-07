#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync::detail;

using std::make_shared;

struct resource {};

struct mocked_condition_variable {
    MOCK_CONST_METHOD0(notify_one, void ());
    MOCK_CONST_METHOD0(notify_all, void ());
    MOCK_CONST_METHOD2(wait_for, std::cv_status (std::unique_lock<std::mutex>&, time_traits::duration));
};

typedef pool_impl<resource, mocked_condition_variable> resource_pool_impl;
typedef resource_pool_impl::get_result get_result;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;

struct sync_resource_pool_impl : Test {};

TEST(sync_resource_pool_impl, create_with_zero_capacity_should_throw_exception) {
    EXPECT_THROW(resource_pool_impl(0, time_traits::duration::max()), error::zero_pool_capacity);
}

TEST(sync_resource_pool_impl, create_with_non_zero_capacity_then_check) {
    const resource_pool_impl pool(1, time_traits::duration::max());
    EXPECT_EQ(pool.capacity(), 1);
}

TEST(sync_resource_pool_impl, create_then_check_size_should_be_0) {
    const resource_pool_impl pool(1, time_traits::duration::max());
    EXPECT_EQ(pool.size(), 0);
}

TEST(sync_resource_pool_impl, create_then_check_available_should_be_0) {
    const resource_pool_impl pool(1, time_traits::duration::max());
    EXPECT_EQ(pool.available(), 0);
}

TEST(sync_resource_pool_impl, create_then_check_used_should_be_0) {
    const resource_pool_impl pool(1, time_traits::duration::max());
    EXPECT_EQ(pool.used(), 0);
}

TEST(sync_resource_pool_impl, create_const_then_check_stats_should_be_0_0_0) {
    const resource_pool_impl pool(1, time_traits::duration::max());
    const sync::stats expected {0, 0, 0};
    const auto actual = pool.stats();

    EXPECT_EQ(actual.size, expected.size);
    EXPECT_EQ(actual.available, expected.available);
    EXPECT_EQ(actual.used, expected.used);
}

TEST(sync_resource_pool_impl, get_one_should_succeed) {
    resource_pool_impl pool_impl(1, time_traits::duration::max());
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
}

TEST(sync_resource_pool_impl, get_one_and_recycle_should_succeed) {
    resource_pool_impl pool_impl(1, time_traits::duration::max());
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
    EXPECT_CALL(pool_impl.has_capacity(), notify_one()).WillOnce(Return());
    pool_impl.recycle(res.second);
}

TEST(sync_resource_pool_impl, get_one_and_waste_should_succeed) {
    resource_pool_impl pool_impl(1, time_traits::duration::max());
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
    EXPECT_CALL(pool_impl.has_capacity(), notify_one()).WillOnce(Return());
    pool_impl.waste(res.second);
}

TEST(sync_resource_pool_impl, get_more_than_capacity_returns_error) {
    resource_pool_impl pool_impl(1, time_traits::duration::max());
    pool_impl.get();
    EXPECT_CALL(pool_impl.has_capacity(), wait_for(_, _)).WillOnce(Return(std::cv_status::timeout));
    EXPECT_EQ(pool_impl.get().first, make_error_code(error::get_resource_timeout));
}

TEST(sync_resource_pool_impl, get_after_disable_capacity_returns_error) {
    resource_pool_impl pool_impl(1, time_traits::duration::max());
    EXPECT_CALL(pool_impl.has_capacity(), notify_all()).WillOnce(Return());
    pool_impl.disable();
    EXPECT_EQ(pool_impl.get().first, make_error_code(error::disabled));
}

struct handle_resource {
    typedef void (resource_pool_impl::*strategy_type)(resource_ptr_list_iterator);

    resource_pool_impl& pool;
    resource_ptr_list_iterator res_it;
    strategy_type strategy;

    handle_resource(resource_pool_impl& pool, resource_ptr_list_iterator res_it, strategy_type strategy)
        : pool(pool), res_it(res_it), strategy(strategy) {}

    std::cv_status operator ()(std::unique_lock<std::mutex>& lock, time_traits::duration) const {
        lock.unlock();
        (pool.*strategy)(res_it);
        lock.lock();
        return std::cv_status::no_timeout;
    }
};

struct recycle_resource : handle_resource {
    recycle_resource(resource_pool_impl& pool, resource_ptr_list_iterator res_it)
        : handle_resource(pool, res_it, &resource_pool_impl::recycle) {}
};

TEST(sync_resource_pool_impl, get_from_pool_and_wait_then_after_recycle_should_allocate) {
    resource_pool_impl pool(1, time_traits::duration::max());
    const get_result& first_res = pool.get();

    EXPECT_EQ(first_res.first, boost::system::error_code());
    EXPECT_NE(first_res.second, resource_ptr_list_iterator());

    InSequence s;

    EXPECT_CALL(pool.has_capacity(), wait_for(_, _)).WillOnce(Invoke(recycle_resource(pool, first_res.second)));
    EXPECT_CALL(pool.has_capacity(), notify_one()).WillOnce(Return());

    const get_result& second_res = pool.get();

    EXPECT_FALSE(second_res.first);
    EXPECT_EQ(second_res.second, first_res.second);
}

struct waste_resource : handle_resource {
    waste_resource(resource_pool_impl& pool, resource_ptr_list_iterator res_it)
        : handle_resource(pool, res_it, &resource_pool_impl::waste) {}
};

TEST(sync_resource_pool_impl, get_from_pool_and_wait_then_after_waste_should_reserve) {
    resource_pool_impl pool(1, time_traits::duration::max());
    const get_result& first_res = pool.get();

    EXPECT_EQ(first_res.first, boost::system::error_code());
    EXPECT_NE(first_res.second, resource_ptr_list_iterator());

    InSequence s;

    EXPECT_CALL(pool.has_capacity(), wait_for(_, _)).WillOnce(Invoke(waste_resource(pool, first_res.second)));
    EXPECT_CALL(pool.has_capacity(), notify_one()).WillOnce(Return());

    const get_result& second_res = pool.get();

    EXPECT_FALSE(second_res.first);
    EXPECT_NE(second_res.second, resource_ptr_list_iterator());
}

struct disable_pool {
    resource_pool_impl& pool;

    disable_pool(resource_pool_impl& pool) : pool(pool) {}

    std::cv_status operator ()(std::unique_lock<std::mutex>& lock, time_traits::duration) const {
        lock.unlock();
        pool.disable();
        lock.lock();
        return std::cv_status::no_timeout;
    }
};

TEST(sync_resource_pool_impl, get_from_pool_with_zero_capacity_then_disable_should_return_error) {
    resource_pool_impl pool(1, time_traits::duration::max());
    const get_result& first = pool.get();

    EXPECT_EQ(first.first, boost::system::error_code());

    InSequence s;

    EXPECT_CALL(pool.has_capacity(), wait_for(_, _)).WillOnce(Invoke(disable_pool(pool)));
    EXPECT_CALL(pool.has_capacity(), notify_all()).WillOnce(Return());

    const get_result& result = pool.get();

    EXPECT_EQ(result.first, make_error_code(error::disabled));
}

TEST(sync_resource_pool_impl, get_one_set_and_recycle_with_zero_idle_timeout_then_get_should_return_empty) {
    resource_pool_impl pool_impl(1, time_traits::duration(0));

    EXPECT_CALL(pool_impl.has_capacity(), notify_one()).WillOnce(Return());

    const get_result first_res = pool_impl.get();
    EXPECT_EQ(first_res.first, boost::system::error_code());
    ASSERT_NE(first_res.second, resource_ptr_list_iterator());
    first_res.second->value = resource {};
    EXPECT_TRUE(first_res.second->value);
    pool_impl.recycle(first_res.second);

    EXPECT_EQ(pool_impl.available(), 1);

    const get_result second_res = pool_impl.get();
    EXPECT_EQ(second_res.first, boost::system::error_code());
    ASSERT_NE(second_res.second, resource_ptr_list_iterator());
    EXPECT_FALSE(second_res.second->value);
}

}
