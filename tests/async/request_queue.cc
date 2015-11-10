#include <yamail/resource_pool/async/detail/request_queue/queue.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool::async::detail::request_queue;

typedef queue<request> request_queue;
typedef boost::shared_ptr<request_queue> request_queue_ptr;

struct async_request_queue_simple : Test {};

TEST(async_request_queue_simple, create_should_succeed) {
    io_service ios;
    request_queue queue(ios, 0);
}

struct async_request_queue_complex : async_test {};

struct callback : base_callback {
    callback(boost::promise<void>& called) : base_callback(called) {}

    void operator ()() const {
        _impl->call();
        _called.set_value();
    }
};

TEST_F(async_request_queue_complex, push_and_wait_queue_should_call_expired) {
    boost::promise<void> called;
    callback expired(called);
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), 1);
    EXPECT_EQ(queue->push(request(), expired, seconds(0)), boost::system::error_code());
    assert_get_nothrow(called);
    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, make_error_code(error::request_queue_is_empty));
}

TEST_F(async_request_queue_complex, push_and_dont_wait_pop_should_returns_request) {
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), 1);
    EXPECT_EQ(queue->push(request(), throw_on_call, seconds(1)), boost::system::error_code());
    EXPECT_FALSE(queue->empty());
    queue->pop();
}

TEST_F(async_request_queue_complex, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), 0);
    EXPECT_EQ(queue->push(request(), throw_on_call, seconds(0)), make_error_code(error::request_queue_overflow));
}

TEST_F(async_request_queue_complex, pop_from_empty_should_return_empty) {
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), 1);
    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, make_error_code(error::request_queue_is_empty));
}

}
