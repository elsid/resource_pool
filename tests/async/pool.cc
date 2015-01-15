#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "pool_tests.hpp"

namespace {

using namespace pool_tests;

typedef pool<resource_ptr> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;
typedef resource_pool::make_handle_ptr make_resource_handle_ptr;

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
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 1, 0);
    const reset_resource_if_need reset_res(make_resource, called);
    pool.get_auto_waste(reset_res);
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_recycle_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_recycle((lambda::bind(reset_res, lambda::_1), lambda::bind(use, lambda::_1)), seconds(1));
    reset_res_called.get_future().get();
    use_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recylce_handle_and_waste_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_recycle((lambda::bind(reset_res, lambda::_1), lambda::bind(use, lambda::_1)), seconds(1));
    reset_res_called.get_future().get();
    use_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_recycle_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_waste((lambda::bind(reset_res, lambda::_1), lambda::bind(use, lambda::_1)), seconds(1));
    reset_res_called.get_future().get();
    use_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_waste_handle_and_waste_should_succeed) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> reset_res_called;
    boost::promise<void> use_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need reset_res(make_resource, reset_res_called);
    const use_handle use(&use_handle::recycle, use_called);
    pool.get_auto_waste((lambda::bind(reset_res, lambda::_1), lambda::bind(use, lambda::_1)), seconds(1));
    reset_res_called.get_future().get();
    use_called.get_future().get();
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
    const check_error first_check(error::none, first_check_called);
    const check_error second_check(error::none, second_check_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1), lambda::bind(first_check, lambda::_1)), seconds(1));
    pool.get_auto_recycle((lambda::bind(second_reset_res, lambda::_1), lambda::bind(second_check, lambda::_1)), seconds(1));
    first_reset_res_called.get_future().get();
    second_reset_res_called.get_future().get();
    first_check_called.get_future().get();
    second_check_called.get_future().get();
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
    const check_error first_check(error::none, first_check_called);
    const check_error second_check(error::none, second_check_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1), lambda::bind(first_check, lambda::_1)), seconds(1));
    pool.get_auto_recycle((lambda::bind(second_reset_res, lambda::_1), lambda::bind(second_check, lambda::_1)), seconds(1));
    first_reset_res_called.get_future().get();
    second_reset_res_called.get_future().get();
    first_check_called.get_future().get();
    second_check_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_two_and_use_queue_should_return_get_resource_timeout_error_for_second) {
    using namespace boost;
    using namespace boost::lambda;
    boost::promise<void> first_reset_res_called;
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool pool(*_io_service, 1, 1);
    const reset_resource_if_need first_reset_res(make_resource, first_reset_res_called);
    const check_error first_check(error::none, first_called);
    const check_error second_check(error::get_resource_timeout, second_called);
    pool.get_auto_recycle((lambda::bind(first_reset_res, lambda::_1), lambda::bind(first_check, lambda::_1)), seconds(1));
    pool.get_auto_recycle(second_check);
    first_reset_res_called.get_future().get();
    first_called.get_future().get();
    second_called.get_future().get();
}

TEST_F(async_resource_pool_complex, get_auto_recycle_handle_from_empty_pool_should_return_error) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 0);
    const check_error check(error::get_resource_timeout, called);
    pool.get_auto_recycle(check);
    called.get_future().get();
}

TEST_F(async_resource_pool_complex, create_my_resoure_handle_should_succeed) {
    boost::promise<void> called;
    resource_pool pool(*_io_service, 0, 1);
    const create_my_resource_handle use(called);
    pool.get_auto_recycle(use);
    called.get_future().get();
}

}
