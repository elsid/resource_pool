#include <gtest/gtest.h>
#include <yamail/resource_pool.hpp>

namespace {

using namespace yamail::resource_pool;

struct resource {};

typedef boost::shared_ptr<resource> resource_ptr;
typedef pool<resource_ptr> resource_pool;
typedef resource_pool::handle_ptr resource_handle_ptr;

class my_resource_handle : public handle_facade<resource_pool> {
public:
    my_resource_handle(const resource_handle_ptr& handle)
            : handle_facade<resource_pool>(handle) {}
};

resource_ptr make_resource() {
    return resource_ptr(new resource);
}

TEST(resource_pool, dummy_create) {
    resource_pool pool;
}

TEST(resource_pool, dummy_create_not_empty) {
    resource_pool pool(42);
}

TEST(resource_pool, dummy_create_not_empty_with_factory) {
    resource_pool pool(42, make_resource);
}

TEST(resource_pool, check_metrics_for_empty) {
    resource_pool pool;
    EXPECT_EQ(pool.capacity(), 0);
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.used(), 0);
    EXPECT_EQ(pool.available(), 0);
}

TEST(resource_pool, check_capacity) {
    const std::size_t capacity = 42;
    resource_pool pool(capacity);
    EXPECT_EQ(pool.capacity(), capacity);
}

TEST(resource_pool, dummy_get_auto_recylce_handle) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
}

TEST(resource_pool, dummy_get_auto_waste_handle) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_waste();
}

TEST(resource_pool, check_metrics_for_not_empty) {
    const std::size_t capacity = 42;
    resource_pool pool(capacity, make_resource);
    EXPECT_EQ(pool.size(), 0);
    EXPECT_EQ(pool.used(), 0);
    EXPECT_EQ(pool.available(), 0);
    {
        resource_handle_ptr handle = pool.get_auto_recycle();
        EXPECT_EQ(pool.size(), 1);
        EXPECT_EQ(pool.used(), 1);
        EXPECT_EQ(pool.available(), 0);
    }
    EXPECT_EQ(pool.size(), 1);
    EXPECT_EQ(pool.used(), 0);
    EXPECT_EQ(pool.available(), 1);
    {
        resource_handle_ptr handle1 = pool.get_auto_recycle();
        resource_handle_ptr handle2 = pool.get_auto_recycle();
        EXPECT_EQ(pool.size(), 2);
        EXPECT_EQ(pool.used(), 2);
        EXPECT_EQ(pool.available(), 0);
    }
    EXPECT_EQ(pool.size(), 2);
    EXPECT_EQ(pool.used(), 0);
    EXPECT_EQ(pool.available(), 2);
    {
        resource_handle_ptr handle = pool.get_auto_waste();
        EXPECT_EQ(pool.size(), 2);
        EXPECT_EQ(pool.used(), 1);
        EXPECT_EQ(pool.available(), 1);
    }
    EXPECT_EQ(pool.size(), 1);
    EXPECT_EQ(pool.used(), 0);
    EXPECT_EQ(pool.available(), 1);
}

TEST(resource_pool, get_auto_recylce_handle_and_recycle) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
    handle->recycle();
}

TEST(resource_pool, get_auto_recylce_handle_and_waste) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
    handle->waste();
}

TEST(resource_pool, get_auto_waste_handle_and_recycle) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_waste();
    handle->recycle();
}

TEST(resource_pool, get_auto_waste_handle_and_waste) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_waste();
    handle->waste();
}

TEST(resource_pool, get_auto_recycle_handle_check_empty) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
    EXPECT_FALSE(handle->empty());
    handle->recycle();
    EXPECT_TRUE(handle->empty());
}

TEST(resource_pool, get_auto_recycle_handle_and_get_recycled_expect_exception) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
    handle->recycle();
    EXPECT_THROW(handle->get(), empty_handle);
}

TEST(resource_pool, get_auto_recycle_handle_and_recycle_recycled_expect_exception) {
    resource_pool pool(1, make_resource);
    resource_handle_ptr handle = pool.get_auto_recycle();
    handle->recycle();
    EXPECT_THROW(handle->recycle(), empty_handle);
}

TEST(resource_pool, get_auto_recycle_handle_from_empty_pool_returns_empty_handle) {
    resource_pool pool(0, make_resource);
    EXPECT_THROW(pool.get_auto_recycle(), get_resource_timeout);
}

TEST(resource_pool, dummy_create_my_resoure_handle) {
    resource_pool pool(1, make_resource);
    my_resource_handle handle(pool.get_auto_recycle());
}

TEST(resource_pool, check_pool_lifetime) {
    resource_handle_ptr handle;
    {
        resource_pool pool(1, make_resource);
        handle = pool.get_auto_recycle();
    }
}

}
