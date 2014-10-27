#include "pool_tests.hpp"

namespace {

using namespace pool_tests;

typedef pool<resource_ptr> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;
typedef resource_pool::make_handle_ptr make_resource_handle_ptr;

struct async_resource_pool_simple : public Test {};

TEST(async_resource_pool_simple, create_should_succeed) {
    io_service ios;
    resource_pool pool(ios);
}

TEST(async_resource_pool_simple, create_not_empty_should_succeed) {
    io_service ios;
    resource_pool pool(ios, 42, 42);
}

TEST(async_resource_pool_simple, create_not_empty_with_factory_should_succeed) {
    io_service ios;
    resource_pool pool(ios, 42, 42, make_resource);
}

TEST(async_resource_pool_simple, check_metrics_for_empty) {
    io_service ios;
    resource_pool pool(ios);
    EXPECT_EQ(pool.capacity(), 0ul);
    EXPECT_EQ(pool.size(), 0ul);
    EXPECT_EQ(pool.used(), 0ul);
    EXPECT_EQ(pool.available(), 0ul);
    EXPECT_EQ(pool.queue_capacity(), 0ul);
    EXPECT_EQ(pool.queue_size(), 0ul);
}

TEST(async_resource_pool_simple, check_capacity) {
    const std::size_t capacity = 42;
    io_service ios;
    resource_pool pool(ios, capacity);
    EXPECT_EQ(pool.capacity(), capacity);
}

struct async_resource_pool_complex : public async_test {};

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 0, make_resource);
    callback do_nothing(called);
    pool.get_auto_recycle(do_nothing);
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 0, make_resource);
    callback do_nothing(called);
    pool.get_auto_waste(do_nothing);
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_recycle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 1, make_resource);
    use_handle use(&use_handle::recycle, called);
    pool.get_auto_recycle(use, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_waste_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 1, make_resource);
    use_handle use(&use_handle::waste, called);
    pool.get_auto_recycle(use, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_recycle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 1, make_resource);
    use_handle use(&use_handle::recycle, called);
    pool.get_auto_waste(use, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_waste_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 1, make_resource);
    use_handle use(&use_handle::waste, called);
    pool.get_auto_waste(use, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_to_wait_create_should_succeed) {
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool pool(*_io_service, 2, 2, make_resource);
    check_error first_check(error::none, first_called);
    check_error second_check(error::none, second_called);
    pool.get_auto_recycle(first_check, seconds(1));
    pool.get_auto_recycle(second_check, seconds(1));
    first_called.get_future().get();
    second_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_to_wait_recycle_should_succeed) {
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool pool(*_io_service, 1, 2, make_resource);
    check_error first_check(error::none, first_called);
    check_error second_check(error::none, second_called);
    pool.get_auto_recycle(first_check, seconds(1));
    pool.get_auto_recycle(second_check, seconds(1));
    first_called.get_future().get();
    second_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_should_return_get_resource_timeout_error_for_second) {
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool pool(*_io_service, 1, 1, make_resource);
    check_error first_check(error::none, first_called);
    check_error second_check(error::get_resource_timeout, second_called);
    pool.get_auto_recycle(first_check, seconds(1));
    pool.get_auto_recycle(second_check);
    first_called.get_future().get();
    second_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recycle_handle_from_empty_pool_should_return_error) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 0, make_resource);
    check_error check(error::get_resource_timeout, called);
    pool.get_auto_recycle(check);
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, create_my_resoure_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 1, make_resource);
    create_my_resource_handle use(called);
    pool.get_auto_recycle(use);
    called.get_future().get();
}

}
