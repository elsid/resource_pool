#include "tests.hpp"

#include <yamail/resource_pool/async/detail/queue.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

namespace asio = boost::asio;

struct mocked_timer {
    MOCK_CONST_METHOD0(expires_at, time_traits::time_point ());
    MOCK_CONST_METHOD0(cancel, void ());
    MOCK_CONST_METHOD1(expires_at, void (const time_traits::time_point&));
    MOCK_CONST_METHOD1(async_wait, void (std::function<void (boost::system::error_code)>));
};

struct timer {
    std::unique_ptr<mocked_timer> impl = std::make_unique<mocked_timer>();

    timer(mocked_io_context&) {}

    time_traits::time_point expires_at() const {
        return impl->expires_at();
    }

    void cancel() const {
        return impl->cancel();
    }

    void expires_at(const time_traits::time_point& v) const {
        return impl->expires_at(v);
    }

    void async_wait(std::function<void (boost::system::error_code)> v) const {
        return impl->async_wait(std::move(v));
    }
};

using request_queue = queue<request, std::mutex, mocked_io_context, timer>;
using request_queue_ptr = std::shared_ptr<request_queue>;

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

using mocked_callback_ptr = std::shared_ptr<mocked_callback>;

struct async_request_queue : Test {
    StrictMock<executor_gmock> executor1;
    mocked_executor executor_wrapper1 {&executor1};
    mocked_io_context io1 {&executor_wrapper1};
    StrictMock<executor_gmock> executor2;
    mocked_executor executor_wrapper2 {&executor2};
    mocked_io_context io2 {&executor_wrapper2};
    mocked_callback_ptr expired;
    std::function<void (error_code)> on_async_wait;

    async_request_queue() : expired(std::make_shared<mocked_callback>()) {}

    request_queue_ptr make_queue(std::size_t capacity) {
        return std::make_shared<request_queue>(capacity);
    }
};

TEST_F(async_request_queue, create_const_with_capacity_1_then_check_capacity_should_be_1) {
    const request_queue queue(1);
    EXPECT_EQ(queue.capacity(), 1u);
}

TEST_F(async_request_queue, create_const_then_check_size_should_be_0) {
    const request_queue queue(1);
    EXPECT_EQ(queue.size(), 0u);
}

TEST_F(async_request_queue, create_const_then_check_empty_should_be_true) {
    const request_queue queue(1);
    EXPECT_EQ(queue.empty(), true);
}

TEST_F(async_request_queue, create_const_then_call_timer_should_succeed) {
    request_queue queue(1);
    EXPECT_NO_THROW(queue.timer(io1));
}

TEST_F(async_request_queue, create_ptr_then_call_shared_from_this_should_return_equal) {
    const request_queue_ptr queue = make_queue(1);
    EXPECT_EQ(queue->shared_from_this(), queue);
}

class callback {
public:
    using result_type = void;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()() const { return impl->call(); }

private:
    mocked_callback_ptr impl;
};

TEST_F(async_request_queue, push_then_timeout_request_queue_should_be_empty) {
    request_queue_ptr queue = make_queue(1);

    InSequence s;

    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    EXPECT_CALL(executor1, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired, call()).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, cancel()).WillOnce(Return());

    ASSERT_TRUE(queue->push(io1, request {0}, callback(expired), time_traits::duration(0)));

    on_async_wait(error_code());

    EXPECT_TRUE(queue->empty());
}

TEST_F(async_request_queue, push_then_pop_should_return_request) {
    request_queue_ptr queue = make_queue(1);

    InSequence s;

    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    EXPECT_CALL(*queue->timer(io1).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*expired, call()).Times(0);

    request req {42};
    EXPECT_TRUE(queue->push(io1, req, callback(expired), time_traits::duration(1)));

    using namespace boost::system::errc;

    on_async_wait(error_code(make_error_code(operation_canceled)));

    EXPECT_FALSE(queue->empty());
    mocked_io_context* io_context;
    request_queue::value_type result;
    EXPECT_TRUE(queue->pop(io_context, result));
    EXPECT_EQ(io_context, &io1);
    EXPECT_EQ(result.value, req.value);
}

TEST_F(async_request_queue, push_into_queue_with_null_capacity_should_return_error) {
    request_queue_ptr queue = make_queue(0);

    const bool result = queue->push(io1, request {0}, callback(expired), time_traits::duration(0));
    EXPECT_FALSE(result);
}

TEST_F(async_request_queue, pop_from_empty_should_return_error) {
    request_queue_ptr queue = make_queue(1);

    EXPECT_TRUE(queue->empty());
    mocked_io_context* io_context;
    request_queue::value_type result;
    EXPECT_FALSE(queue->pop(io_context, result));
}

TEST_F(async_request_queue, push_twice_with_different_io_contexts_then_pop_twice_should_return_both_requests) {
    auto& expired1 = expired;
    auto expired2 = std::make_shared<mocked_callback>();
    const auto queue = make_queue(2);

    Sequence s;

    (void) queue->timer(io1);
    (void) queue->timer(io2);

    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*expired1, call()).Times(0);
    EXPECT_CALL(*expired2, call()).Times(0);

    request req1 {42};
    EXPECT_TRUE(queue->push(io1, req1, callback(expired1), time_traits::duration(1)));

    request req2 {13};
    EXPECT_TRUE(queue->push(io2, req2, callback(expired2), time_traits::duration(1)));

    using namespace boost::system::errc;

    ASSERT_FALSE(queue->empty());

    mocked_io_context* io_context1;
    request_queue::value_type result1;
    EXPECT_TRUE(queue->pop(io_context1, result1));
    EXPECT_EQ(io_context1, &io1);
    EXPECT_EQ(result1.value, req1.value);

    mocked_io_context* io_context2;
    request_queue::value_type result2;
    EXPECT_TRUE(queue->pop(io_context2, result2));
    EXPECT_EQ(io_context2, &io2);
    EXPECT_EQ(result2.value, req2.value);
}

TEST_F(async_request_queue, push_twice_with_different_io_contexts_where_second_expires_before_first_then_pop_twice_should_return_both_requests) {
    auto& expired1 = expired;
    auto expired2 = std::make_shared<mocked_callback>();
    const auto queue = make_queue(2);

    Sequence s;

    (void) queue->timer(io1);
    (void) queue->timer(io2);

    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, async_wait(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*expired1, call()).Times(0);
    EXPECT_CALL(*expired2, call()).Times(0);

    request req1 {42};
    EXPECT_TRUE(queue->push(io1, req1, callback(expired1), time_traits::duration::max()));

    request req2 {13};
    EXPECT_TRUE(queue->push(io2, req2, callback(expired2), time_traits::duration::max() / 2));

    using namespace boost::system::errc;

    ASSERT_FALSE(queue->empty());

    mocked_io_context* io_context1;
    request_queue::value_type result1;
    EXPECT_TRUE(queue->pop(io_context1, result1));
    EXPECT_EQ(io_context1, &io1);
    EXPECT_EQ(result1.value, req1.value);

    mocked_io_context* io_context2;
    request_queue::value_type result2;
    EXPECT_TRUE(queue->pop(io_context2, result2));
    EXPECT_EQ(io_context2, &io2);
    EXPECT_EQ(result2.value, req2.value);
}

TEST_F(async_request_queue, push_twice_with_different_io_serivices_and_timeout_both_requests_then_queue_should_be_empty) {
    auto& expired1 = expired;
    auto& on_async_wait1 = on_async_wait;
    auto expired2 = std::make_shared<mocked_callback>();
    std::function<void (error_code)> on_async_wait2;
    const auto queue = make_queue(2);

    Sequence s;

    (void) queue->timer(io1);
    (void) queue->timer(io2);

    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).InSequence(s).WillOnce(SaveArg<0>(&on_async_wait1));
    EXPECT_CALL(*queue->timer(io1).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, async_wait(_)).InSequence(s).WillOnce(SaveArg<0>(&on_async_wait1));
    EXPECT_CALL(executor1, post(_)).InSequence(s).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired1, call()).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, expires_at(_)).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, async_wait(_)).InSequence(s).WillOnce(SaveArg<0>(&on_async_wait2));
    EXPECT_CALL(executor2, post(_)).InSequence(s).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*expired2, call()).InSequence(s).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io1).impl, cancel()).WillOnce(Return());
    EXPECT_CALL(*queue->timer(io2).impl, cancel()).WillOnce(Return());

    ASSERT_TRUE(queue->push(io1, request {42}, callback(expired1), time_traits::duration(0)));
    ASSERT_TRUE(queue->push(io2, request {13}, callback(expired2), time_traits::duration(0)));

    on_async_wait1(error_code());
    on_async_wait2(error_code());

    EXPECT_TRUE(queue->empty());
}

}
