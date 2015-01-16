#include <boost/array.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "pool_tests.hpp"

namespace {

using namespace pool_tests;

using boost::range::transform;
using boost::range::for_each;
using boost::chrono::duration_cast;

typedef yamail::resource_pool::async::detail::request_queue::clock clock;
typedef boost::asio::steady_timer timer;

struct load_test_async_resource_pool : public async_test {};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

class finite_pereodic_work {
public:
    finite_pereodic_work(
            io_service& io_service,
            const boost::function<void ()>& job,
            const time_duration& job_interval)
            : _io_service(io_service), _job(job), _job_interval(job_interval),
              _timer(io_service) {}

    boost::unique_future<boost::system::error_code> start(const std::size_t count) {
        perform(count, boost::system::error_code());
        return _finished.get_future();
    }

private:
    io_service& _io_service;
    boost::function<void ()> _job;
    time_duration _job_interval;
    boost::promise<boost::system::error_code> _finished;
    timer _timer;

    void perform(const std::size_t count, const boost::system::error_code& ec) {
        if (ec != boost::system::errc::success) {
            _timer.cancel();
            _finished.set_value(ec);
            return;
        }
        if (count == 0) {
            _finished.set_value(boost::system::error_code());
            return;
        }
        _timer.expires_from_now(_job_interval);
        _timer.async_wait(bind(&finite_pereodic_work::perform, this, count - 1, _1));
        _io_service.post(_job);
    }
};

void get_result(boost::promise<void>& called) {
    called.get_future().get();
}

template <std::size_t count>
class get_auto_recycle_impl {
public:
    get_auto_recycle_impl(resource_pool& pool, const time_duration& wait_duration)
            : _pool(pool), _wait_duration(wait_duration),
              _called_it(_called.begin()) {}

    void operator ()() {
        unique_lock lock(_mutex);
        if (_called_it == _called.end()) {
            throw std::logic_error("too many calls");
        }
        boost::promise<void>& reset_called = *_called_it;
        ++_called_it;
        boost::promise<void>& check_called = *_called_it;
        ++_called_it;
        lock.unlock();
        const reset_resource_if_need reset(make_resource, reset_called);
        const check_error check(boost::system::error_code(), check_called);
        using namespace boost;
        using namespace boost::lambda;
        _pool.get_auto_recycle((lambda::bind(reset, lambda::_1, lambda::_2),
             lambda::bind(check, lambda::_1, lambda::_2)), _wait_duration);
    }

    void wait() {
        unique_lock lock(_mutex);
        for_each(_called, get_result);
    }

private:
    typedef boost::array<boost::promise<void>, 2 * count> called_array;

    resource_pool& _pool;
    time_duration _wait_duration;
    boost::mutex _mutex;
    called_array _called;
    typename called_array::iterator _called_it;
};

template <std::size_t count>
class get_auto_recycle {
public:
    get_auto_recycle(resource_pool& pool, const time_duration& wait_duration)
            : _impl(new get_auto_recycle_impl<count>(pool, wait_duration)) {}

    void operator ()() { _impl->operator ()(); }
    void wait() { _impl->wait(); }

private:
    boost::shared_ptr<get_auto_recycle_impl<count> > _impl;
};

TEST_F(load_test_async_resource_pool, perform_many_get_auto_recycle_should_succeed) {
    const std::size_t threads_count = 8;
    const time_duration load_interval = microseconds(50);
    const time_duration wait_duration = milliseconds(50);
    const std::size_t pool_capacity = 10;
    const std::size_t pool_queue_capacity = 1000;
    const std::size_t load_count = 100000;
    io_service ios;
    io_service::work work(ios);
    boost::thread_group thread_pool;
    for (std::size_t n = 0; n < threads_count; ++n) {
        thread_pool.create_thread(bind(&io_service::run, ref(ios)));
    }
    resource_pool pool(*_io_service, pool_capacity, pool_queue_capacity);
    get_auto_recycle<load_count> do_load(pool, wait_duration);
    finite_pereodic_work load(ref(ios), do_load, load_interval);
    EXPECT_EQ(load.start(load_count).get(), boost::system::error_code());
    do_load.wait();
    ios.stop();
    thread_pool.join_all();
}

}
