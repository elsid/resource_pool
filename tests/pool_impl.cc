#include <gtest/gtest.h>
#include <yamail/resource_pool.hpp>

namespace {

using namespace yamail::resource_pool;

struct resource {};

typedef boost::shared_ptr<resource> resource_ptr;
typedef pool<resource_ptr> resource_pool;
typedef resource_pool::pool_impl resource_pool_impl;
typedef resource_pool_impl::get_result get_result;

resource_ptr make_resource() {
    return resource_ptr(new resource);
}

TEST(resource_pool, get_one_and_recycle_succeed) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.recycle(*res.second);
}

TEST(resource_pool_impl, get_one_and_waste_succeed) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.waste(*res.second);
}

TEST(resource_pool_impl, fill_and_check_metrics) {
    const std::size_t capacity = 42;
    resource_pool_impl pool_impl(capacity, make_resource);
    pool_impl.fill();
    EXPECT_EQ(pool_impl.capacity(), capacity);
    EXPECT_EQ(pool_impl.size(), capacity);
    EXPECT_EQ(pool_impl.available(), capacity);
    EXPECT_EQ(pool_impl.used(), 0);
}

TEST(resource_pool_impl, fill_then_clear_and_check_metrics) {
    const std::size_t capacity = 42;
    resource_pool_impl pool_impl(capacity, make_resource);
    pool_impl.fill();
    pool_impl.clear();
    EXPECT_EQ(pool_impl.capacity(), capacity);
    EXPECT_EQ(pool_impl.size(), 0);
    EXPECT_EQ(pool_impl.available(), 0);
    EXPECT_EQ(pool_impl.used(), 0);
}

TEST(resource_pool_impl, get_more_than_capacity_returns_empty_resource) {
    resource_pool_impl pool_impl(1, make_resource);
    pool_impl.get();
    EXPECT_EQ(pool_impl.get().first, error::get_resource_timeout);
}

TEST(resource_pool_impl, put_resource_not_from_pool_expect_exception) {
    resource_pool_impl pool_impl(1, make_resource);
    resource_ptr res = make_resource();
    EXPECT_THROW(pool_impl.recycle(res), error::resource_not_from_pool);
    EXPECT_THROW(pool_impl.waste(res), error::resource_not_from_pool);
}

TEST(resource_pool_impl, return_recycled_resource_expect_exception) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.recycle(*res.second);
    EXPECT_THROW(pool_impl.recycle(*res.second), error::add_existing_resource);
    EXPECT_THROW(pool_impl.waste(*res.second), error::add_existing_resource);
}

TEST(resource_pool_impl, return_wasted_resource_expect_exception) {
    resource_pool_impl pool_impl(1, make_resource);
    get_result res = pool_impl.get();
    pool_impl.waste(*res.second);
    EXPECT_THROW(pool_impl.recycle(*res.second), error::resource_not_from_pool);
    EXPECT_THROW(pool_impl.waste(*res.second), error::resource_not_from_pool);
}

}
