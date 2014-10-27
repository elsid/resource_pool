#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <yamail/resource_pool.hpp>

namespace tests {

using namespace testing;
using namespace yamail::resource_pool;

using boost::ref;
using boost::bind;
using boost::make_shared;
using boost::chrono::seconds;
using boost::chrono::milliseconds;
using boost::chrono::microseconds;
using boost::chrono::nanoseconds;
using boost::asio::io_service;

struct resource {};
struct request {};

typedef boost::unique_lock<boost::mutex> unique_lock;
typedef boost::shared_ptr<resource> resource_ptr;
typedef boost::shared_ptr<io_service> io_service_ptr;
typedef boost::unique_lock<boost::mutex> unique_lock;
typedef boost::lock_guard<boost::mutex> lock_guard;
typedef boost::chrono::system_clock::duration time_duration;

class async_test : public Test {
public:
    void SetUp() {
        _io_service = make_shared<io_service>();
        _work = make_shared<io_service::work>(ref(*_io_service));
        _thread_pool.create_thread(bind(&io_service::run, _io_service));
    }

    void TearDown() {
        _io_service->stop();
        _thread_pool.join_all();
    }

protected:
    io_service_ptr _io_service;
    boost::thread_group _thread_pool;
    boost::shared_ptr<io_service::work> _work;
};

struct mocked_callback {
    MOCK_CONST_METHOD0(call, void ());
};

class base_callback {
public:
    typedef void result_type;

    base_callback(boost::promise<void>& called)
            : _called(called), _impl(make_shared<mocked_callback>()) {
        EXPECT_CALL(*_impl, call()).WillOnce(Return());
    }

    virtual ~base_callback() {}

protected:
    boost::promise<void>& _called;
    boost::shared_ptr<mocked_callback> _impl;
};

inline void throw_on_call() {
    throw std::logic_error("unexpected call");
}

}
