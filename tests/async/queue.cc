#include "tests.hpp"

#include <yamail/resource_pool/async/detail/queue.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

struct mocked_timer {
    mocked_timer(mocked_io_service&) {}

    MOCK_CONST_METHOD0(expires_at, time_traits::time_point ());
    MOCK_CONST_METHOD1(expires_at, void (const time_traits::time_point&));
    MOCK_CONST_METHOD1(async_wait, void (boost::function<void (boost::system::error_code)>));
};

typedef queue<request, mocked_io_service, mocked_timer> request_queue;
typedef std::shared_ptr<request_queue> request_queue_ptr;

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

typedef std::shared_ptr<mocked_callback> mocked_callback_ptr;

struct async_request_queue : Test {
    mocked_io_service ios;
    mocked_callback_ptr expired;
    boost::function<void (error_code)> on_async_wait;

    async_request_queue() : expired(std::make_shared<mocked_callback>()) {}

    request_queue_ptr make_queue(std::size_t capacity) {
        return std::make_shared<request_queue>(ref(ios), capacity);
    }
};

TEST_F(async_request_queue, create_const_with_capacity_1_then_check_capacity_should_be_1) {
    const request_queue queue(ios, 1);
    EXPECT_EQ(queue.capacity(), 1);
}

TEST_F(async_request_queue, create_const_then_check_size_should_be_0) {
    const request_queue queue(ios, 1);
    EXPECT_EQ(queue.size(), 0);
}

TEST_F(async_request_queue, create_const_then_check_empty_should_be_true) {
    const request_queue queue(ios, 1);
    EXPECT_EQ(queue.empty(), true);
}

TEST_F(async_request_queue, create_const_then_call_timer_should_succeed) {
    const request_queue queue(ios, 1);
    EXPECT_NO_THROW(queue.timer());
}

TEST_F(async_request_queue, create_ptr_then_call_shared_from_this_should_return_equal) {
    const request_queue_ptr queue = make_queue(1);
    EXPECT_EQ(queue->shared_from_this(), queue);
}

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

    time_traits::time_point expire_time;

    InSequence s;

    EXPECT_CALL(queue->timer(), expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    EXPECT_CALL(queue->timer(), async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));

    request req;
    req.value = 0;
    ASSERT_TRUE(queue->push(req, callback(expired), time_traits::duration(0)));

    EXPECT_CALL(queue->timer(), expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired, call()).WillOnce(Return());

    on_async_wait(error_code());

    EXPECT_TRUE(queue->empty());
}

TEST_F(async_request_queue, push_then_pop_should_return_request) {
    request_queue_ptr queue = make_queue(1);

    InSequence s;

    EXPECT_CALL(queue->timer(), expires_at(_)).WillOnce(Return());
    EXPECT_CALL(queue->timer(), async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    EXPECT_CALL(*expired, call()).Times(0);

    request req;
    req.value = 42;
    EXPECT_TRUE(queue->push(req, callback(expired), time_traits::duration(1)));

    using namespace boost::system::errc;

    on_async_wait(error_code(make_error_code(operation_canceled)));

    EXPECT_FALSE(queue->empty());
    request_queue::value_type result;
    EXPECT_TRUE(queue->pop(result));
    EXPECT_EQ(result.value, req.value);
}

TEST_F(async_request_queue, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_queue(0);

    request req;
    req.value = 0;
    const bool result = queue->push(req, callback(expired), time_traits::duration(0));
    EXPECT_FALSE(result);
}

TEST_F(async_request_queue, pop_from_empty_should_return_error) {
    request_queue_ptr queue = make_queue(1);

    EXPECT_TRUE(queue->empty());
    request_queue::value_type result;
    EXPECT_FALSE(queue->pop(result));
}

}
