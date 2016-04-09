#include <yamail/resource_pool/async/detail/queue.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail::request_queue;

typedef queue<request, mocked_io_service, mocked_timer> request_queue;
typedef boost::shared_ptr<request_queue> request_queue_ptr;

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

struct async_request_queue : Test {
    mocked_io_service ios;
    boost::shared_ptr<mocked_timer> timer;
    mocked_callback_ptr expired;
    boost::function<void (error_code)> on_async_wait;

    async_request_queue() : timer(new mocked_timer()), expired(make_shared<mocked_callback>()) {}

    request_queue_ptr make_queue(std::size_t capacity) {
        return make_shared<request_queue>(ref(ios), timer, capacity);
    }
};

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()() const { return impl->call(); }

private:
    mocked_callback_ptr impl;
};

TEST_F(async_request_queue, push_then_timeout_request_queue_should_be_empty) {
    request_queue_ptr queue = make_queue(1);

    time_point expire_time;

    InSequence s;

    EXPECT_CALL(*timer, expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));

    ASSERT_EQ(queue->push(request(), callback(expired), seconds(0)), error_code());

    EXPECT_CALL(*timer, expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired, call()).WillOnce(Return());

    on_async_wait(error_code());

    EXPECT_TRUE(queue->empty());
}

TEST_F(async_request_queue, push_then_pop_should_return_request) {
    request_queue_ptr queue = make_queue(1);

    InSequence s;

    EXPECT_CALL(*timer, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(Return());
    EXPECT_CALL(*expired, call()).Times(0);

    EXPECT_EQ(queue->push(request(), callback(expired), seconds(1)), error_code());

    EXPECT_FALSE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, error_code());
}

TEST_F(async_request_queue, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_queue(0);

    const error_code& result = queue->push(request(), callback(expired), seconds(0));
    EXPECT_EQ(result, make_error_code(error::request_queue_overflow));
}

TEST_F(async_request_queue, pop_from_empty_should_return_error) {
    request_queue_ptr queue = make_queue(1);

    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, make_error_code(error::request_queue_is_empty));
}

}
