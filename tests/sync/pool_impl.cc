#include <boost/make_shared.hpp>
#include <gtest/gtest.h>
#include <yamail/resource_pool.hpp>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync::detail;

using boost::make_shared;

struct resource {};

typedef boost::shared_ptr<resource> resource_ptr;
typedef pool_impl<resource> resource_pool_impl;
typedef resource_pool_impl::get_result get_result;
typedef resource_pool_impl::resource_ptr_list_iterator resource_ptr_list_iterator;

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

struct sync_resource_pool_impl : Test {};

TEST(sync_resource_pool_impl, get_one_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, error::none);
    EXPECT_EQ(res.second, boost::none);
}

TEST(sync_resource_pool_impl, get_one_and_recycle_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, error::none);
    EXPECT_EQ(res.second, boost::none);
    const resource_ptr_list_iterator res_it = pool_impl.add(make_resource());
    pool_impl.recycle(res_it);
}

TEST(sync_resource_pool_impl, get_one_and_waste_should_succeed) {
    resource_pool_impl pool_impl(1);
    const get_result res = pool_impl.get();
    EXPECT_EQ(res.first, error::none);
    EXPECT_EQ(res.second, boost::none);
    const resource_ptr_list_iterator res_it = pool_impl.add(make_resource());
    pool_impl.waste(res_it);
}

TEST(sync_resource_pool_impl, get_more_than_capacity_should_throw_exception) {
    resource_pool_impl pool_impl(1);
    pool_impl.get();
    EXPECT_EQ(pool_impl.get().first, error::get_resource_timeout);
}

}
