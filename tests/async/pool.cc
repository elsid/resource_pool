#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "pool_tests.hpp"

namespace {

using namespace pool_tests;

typedef pool<resource> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;

struct async_resource_pool_simple : public Test {};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

TEST(async_resource_pool_simple, create_should_succeed) {
    io_service ios;
    resource_pool pool(ios);
}

TEST(async_resource_pool_simple, create_not_empty_should_succeed) {
    io_service ios;
    resource_pool pool(ios, 42, 42);
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
    resource_pool pool(*_io_service, 1, 0);
    const reset_resource_if_need reset_res(make_resource, called);
    pool.get_auto_recycle(reset_res);
    assert_get_nothrow(called);
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 0);
    const reset_resource_if_need reset_res(make_resource, called);
    pool.get_auto_waste(reset_res);
    assert_get_nothrow(called);
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_recycle_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_recycle((lambda::bind(reset_res, lambda::_1, lambda::_2),
        lambda::bind(use, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(reset_res_called);
    assert_get_nothrow(use_called);
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_waste_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_recycle((lambda::bind(reset_res, lambda::_1, lambda::_2),
        lambda::bind(use, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(reset_res_called);
    assert_get_nothrow(use_called);
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_recycle_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_waste((lambda::bind(reset_res, lambda::_1, lambda::_2),
        lambda::bind(use, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(reset_res_called);
    assert_get_nothrow(use_called);
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_waste_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_waste((lambda::bind(reset_res, lambda::_1, lambda::_2),
        lambda::bind(use, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(reset_res_called);
    assert_get_nothrow(use_called);
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_to_wait_create_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> first_reset_res_called;
    boost::promise<void> second_reset_res_called;
    boost::promise<void> first_check_called;
    boost::promise<void> second_check_called;
    resource_pool pool(*_io_service, 2, 2);
    const reset_resource_if_need first_reset_res(make_resource, first_reset_res_called);
    const reset_resource_if_need second_reset_res(make_resource, second_reset_res_called);
    const check_error first_check(boost::system::error_code(), first_check_called);
    const check_error second_check(boost::system::error_code(), second_check_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1, lambda::_2),
        lambda::bind(first_check, lambda::_1, lambda::_2)), seconds(1));
    pool.get_auto_recycle((lambda::bind(second_reset_res, lambda::_1, lambda::_2),
        lambda::bind(second_check, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(first_reset_res_called);
    assert_get_nothrow(second_reset_res_called);
    assert_get_nothrow(first_check_called);
    assert_get_nothrow(second_check_called);
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_to_wait_recycle_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> first_reset_res_called;
    boost::promise<void> second_reset_res_called;
    boost::promise<void> first_check_called;
    boost::promise<void> second_check_called;
    resource_pool pool(*_io_service, 1, 2);
    const reset_resource_if_need first_reset_res(make_resource, first_reset_res_called);
    const reset_resource_if_need second_reset_res(make_resource, second_reset_res_called);
    const check_error first_check(boost::system::error_code(), first_check_called);
    const check_error second_check(boost::system::error_code(), second_check_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1, lambda::_2),
        lambda::bind(first_check, lambda::_1, lambda::_2)), seconds(1));
    pool.get_auto_recycle((lambda::bind(second_reset_res, lambda::_1, lambda::_2),
        lambda::bind(second_check, lambda::_1, lambda::_2)), seconds(1));
    assert_get_nothrow(first_reset_res_called);
    assert_get_nothrow(second_reset_res_called);
    assert_get_nothrow(first_check_called);
    assert_get_nothrow(second_check_called);
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_should_return_get_resource_timeout_error_for_second) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> first_reset_res_called;
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need first_reset_res(make_resource, first_reset_res_called);
    const check_error first_check(boost::system::error_code(), first_called);
    const check_error second_check(make_error_code(error::get_resource_timeout), second_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1, lambda::_2),
        lambda::bind(first_check, lambda::_1, lambda::_2)), seconds(1));
    pool.get_auto_recycle(second_check);
    assert_get_nothrow(first_reset_res_called);
    assert_get_nothrow(first_called);
    assert_get_nothrow(second_called);
}

TEST_F(async_resource_pool_complex, get_auto_recycle_handle_from_empty_pool_should_return_error) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 0);
    const check_error check(make_error_code(error::get_resource_timeout), called);
    pool.get_auto_recycle(check);
    assert_get_nothrow(called);
}

TEST_F(async_resource_pool_complex, create_my_resoure_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 1);
    const create_my_resource_handle use(called);
    pool.get_auto_recycle(use);
    assert_get_nothrow(called);
}

}
