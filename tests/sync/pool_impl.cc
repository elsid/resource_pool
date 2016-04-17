#include <boost/make_shared.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <yamail/resource_pool.hpp>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync::detail;

using boost::make_shared;

struct resource {};

typedef boost::chrono::system_clock::duration time_duration;

struct mocked_condition_variable {
    MOCK_CONST_METHOD0(notify_one, void ());
    MOCK_CONST_METHOD0(notify_all, void ());
    MOCK_CONST_METHOD3(wait_for, bool (boost::unique_lock<boost::mutex>&, time_duration, const boost::function<bool ()>&));
};

typedef boost::shared_ptr<resource> resource_ptr;
typedef pool_impl<resource, mocked_condition_variable> resource_pool_impl;
typedef resource_pool_impl::get_result get_result;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

struct sync_resource_pool_impl : Test {};

TEST(sync_resource_pool_impl, create_with_zero_capacity_should_throw_exception) {
    EXPECT_THROW(resource_pool_impl(0), error::zero_pool_capacity);
}

TEST(sync_resource_pool_impl, get_one_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
}

TEST(sync_resource_pool_impl, get_one_and_recycle_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
    EXPECT_CALL(pool_impl.has_available_cv(), notify_one()).WillOnce(Return());
    pool_impl.recycle(res.second);
}

TEST(sync_resource_pool_impl, get_one_and_waste_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, boost::system::error_code());
    EXPECT_NE(res.second, resource_ptr_list_iterator());
    EXPECT_CALL(pool_impl.has_available_cv(), notify_one()).WillOnce(Return());
    pool_impl.waste(res.second);
}

TEST(sync_resource_pool_impl, get_more_than_capacity_returns_error) {
    resource_pool_impl pool_impl(1);
    pool_impl.get();
    EXPECT_CALL(pool_impl.has_available_cv(), wait_for(_, _, _)).WillOnce(Return(false));
    EXPECT_EQ(pool_impl.get().first, make_error_code(error::get_resource_timeout));
}

TEST(sync_resource_pool_impl, get_after_disable_capacity_returns_error) {
    resource_pool_impl pool_impl(1);
    EXPECT_CALL(pool_impl.has_available_cv(), notify_all()).WillOnce(Return());
    pool_impl.disable();
    EXPECT_EQ(pool_impl.get().first, make_error_code(error::disabled));
}

}
