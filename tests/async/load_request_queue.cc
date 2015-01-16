#include <boost/array.hpp>
#include <boost/range/algorithm/for_each.hpp>

#include <yamail/resource_pool/async/detail/request_queue/queue.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool::async::detail::request_queue;

typedef queue<request> request_queue;
typedef boost::shared_ptr<request_queue> request_queue_ptr;

struct load_test_async_request_queue : async_test {};

TEST_F(load_test_async_request_queue, push_many_should_pop_all_not_empty) {
    const std::size_t count = 1000;
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), count);
    for (std::size_t n = 0; n < count; ++n) {
        EXPECT_EQ(queue->push(request(), throw_on_call, seconds(count - n)), error::none);
    }
    EXPECT_EQ(queue->size(), count);
    for (std::size_t n = 0; n < count; ++n) {
        EXPECT_FALSE(queue->empty());
        queue->pop();
    }
}

namespace push_many_and_sleep_should_pop_empty {

void do_nothing(boost::promise<void>& called) {
    called.set_value();
}

void push_request(request_queue_ptr queue, const time_duration& wait_duration, boost::promise<void>& called) {
    EXPECT_EQ(queue->push(request(), bind(do_nothing, ref(called)), wait_duration), error::none);
}

void get_result(boost::promise<void>& called) {
    called.get_future().get();
}

TEST_F(load_test_async_request_queue, push_many_and_sleep_should_pop_empty) {
    const std::size_t count = 1000;
    milliseconds wait_duration(10);
    request_queue_ptr queue = make_shared<request_queue>(ref(*_io_service), count);
    boost::array<boost::promise<void>, count> called;
    for_each(called, bind(push_request, queue, wait_duration, _1));
    for_each(called, bind(get_result, _1));
    EXPECT_TRUE(queue->empty());
    const request_queue::pop_result& result = queue->pop();
    EXPECT_EQ(result.error, error::request_queue_is_empty);
}

}
}
