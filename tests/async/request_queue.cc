#include <yamail/resource_pool/async/detail/request_queue/queue.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail::request_queue;

typedef queue<request, mocked_io_service, mocked_timer> request_queue;
typedef boost::shared_ptr<request_queue> request_queue_ptr;

using boost::system::error_code;

struct async_request_queue : Test {
    mocked_io_service ios;
    boost::shared_ptr<mocked_timer> timer;

    async_request_queue() : timer(new mocked_timer()) {}

    request_queue_ptr make_queue(std::size_t capacity) {
        return make_shared<request_queue>(ref(ios), timer, capacity);
    }
};

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()() const { return impl->call(); }

private:
    mocked_callback_ptr impl;
};

TEST_F(async_request_queue, push_and_wait_queue_should_call_expired) {
    request_queue_ptr queue = make_queue(1);

    time_point expire_time;
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    boost::function<void (error_code)> on_async_wait;
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    mocked_callback_ptr expired_impl = make_shared<mocked_callback>();
    callback expired(expired_impl);
    EXPECT_EQ(queue->push(request(), expired, seconds(0)), error_code());

    EXPECT_CALL(*timer, expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired_impl, call()).WillOnce(Return());
    on_async_wait(error_code());

    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, make_error_code(error::request_queue_is_empty));
}

TEST_F(async_request_queue, push_and_dont_wait_pop_should_returns_request) {
    request_queue_ptr queue = make_queue(1);

    time_point expire_time;
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    boost::function<void (error_code)> on_async_wait;
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    mocked_callback_ptr expired_impl = make_shared<mocked_callback>();
    callback expired(expired_impl);
    EXPECT_CALL(*expired_impl, call()).Times(0);
    EXPECT_EQ(queue->push(request(), expired, seconds(1)), error_code());

    EXPECT_FALSE(queue->empty());
    queue->pop();
}

TEST_F(async_request_queue, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_queue(0);
    mocked_callback_ptr expired_impl = make_shared<mocked_callback>();
    callback expired(expired_impl);
    EXPECT_CALL(*expired_impl, call()).Times(0);
    EXPECT_EQ(queue->push(request(), expired, seconds(0)), make_error_code(error::request_queue_overflow));
}

TEST_F(async_request_queue, pop_from_empty_should_return_empty) {
    request_queue_ptr queue = make_queue(1);
    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, make_error_code(error::request_queue_is_empty));
}

}
