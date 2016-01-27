#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <list>

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

using detail::clock;

template <class Value,
          class IoService = boost::asio::io_service,
          class Timer = boost::asio::basic_waitable_timer<clock> >
class pool : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Timer timer_t;
    typedef detail::pool_impl<value_type, io_service_t, timer_t> pool_impl;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef resource_pool::handle<pool> handle;
    typedef boost::shared_ptr<handle> handle_ptr;
    typedef boost::function<void (const boost::system::error_code&,
        handle_ptr)> callback;

    pool(io_service_t& io_service,
         std::size_t capacity = 0,
         std::size_t queue_capacity = 0)
            : _impl(boost::make_shared<pool_impl>(
                boost::ref(io_service),
                boost::make_shared<timer_t>(boost::ref(io_service)),
                capacity,
                queue_capacity)) {}

    pool(io_service_t& io_service,
         boost::shared_ptr<timer_t> timer,
         std::size_t capacity = 0,
         std::size_t queue_capacity = 0)
            : _impl(boost::make_shared<pool_impl>(
                boost::ref(io_service),
                timer,
                capacity,
                queue_capacity)) {}

    ~pool() { _impl->disable(); }

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    std::size_t queue_capacity() const { return _impl->queue_capacity(); }
    std::size_t queue_size() const { return _impl->queue_size(); }
    bool queue_empty() const { return _impl->queue_empty(); }

    void get_auto_waste(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::waste, wait_duration);
    }

    void get_auto_recycle(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::recycle, wait_duration);
    }

private:
    typedef typename pool_impl::list_iterator_opt list_iterator_opt;
    typedef typename handle::strategy strategy;
    typedef typename boost::shared_ptr<pool_impl> pool_impl_ptr;

    pool_impl_ptr _impl;

    void get(callback call, strategy use_strategy,
            const time_duration& wait_duration) {
        _impl->get(bind(make_handle, _impl, call, use_strategy, _1, _2),
            wait_duration);
    }

    static void make_handle(pool_impl_ptr impl, callback call,
            strategy use_strategy, const boost::system::error_code& ec,
            const list_iterator_opt& res) {
        if (ec) {
            return call(ec, handle_ptr());
        }
        handle_ptr res_handle;
        boost::system::error_code error;
        try {
            res_handle.reset(new handle(impl, use_strategy, res));
        } catch (const std::exception&) {
            error = make_error_code(error::exception);
        }
        call(error, res_handle);
    }
};

} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
