#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

#include <iostream>

namespace yamail {
namespace resource_pool {
namespace async {

template <class Value>
struct default_pool_queue {
    typedef Value value_type;
    typedef boost::asio::io_service io_service_t;
    typedef std::shared_ptr<value_type> pointer;
    typedef resource_pool::detail::idle<pointer> idle;
    typedef std::list<idle> list;
    typedef typename list::iterator list_iterator;
    typedef std::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef detail::queue<callback, io_service_t, time_traits::timer> type;
};

template <class Value, class IoService>
struct default_pool_impl {
    typedef typename detail::pool_impl<
        Value,
        IoService,
        detail::abort,
        typename default_pool_queue<Value>::type
    > type;
};

template <class Value,
          class IoService = boost::asio::io_service,
          class Impl = typename default_pool_impl<Value, IoService>::type >
class pool : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Impl pool_impl;
    typedef resource_pool::handle<pool> handle;
    typedef typename pool_impl::on_catch_handler_exception_type on_catch_handler_exception_type;

    pool(io_service_t& io_service,
         std::size_t capacity,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max(),
         const on_catch_handler_exception_type& on_catch_handler_exception = detail::abort())
            : _impl(std::make_shared<pool_impl>(
                io_service,
                capacity,
                queue_capacity,
                idle_timeout,
                on_catch_handler_exception)) {}

    ~pool() { _impl->disable(); }

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    const pool_impl& impl() const { return *_impl; }

    template <class Callback>
    void get_auto_waste(const Callback& call,
                        time_traits::duration wait_duration = time_traits::duration(0)) {
        return get(call, &handle::waste, wait_duration);
    }

    template <class Callback>
    void get_auto_recycle(const Callback& call,
                          time_traits::duration wait_duration = time_traits::duration(0)) {
        return get(call, &handle::recycle, wait_duration);
    }

private:
    typedef typename pool_impl::list_iterator list_iterator;
    typedef typename handle::strategy strategy;
    typedef typename std::shared_ptr<pool_impl> pool_impl_ptr;

    pool_impl_ptr _impl;

    template <class Callback>
    void get(Callback call, strategy use_strategy, time_traits::duration wait_duration) {
        const auto impl = _impl;
        const auto on_get = [impl, use_strategy, call] (const boost::system::error_code& ec, list_iterator res) mutable {
            if (ec) {
                call(ec, handle());
            } else {
                call(ec, handle(impl, use_strategy, res));
            }
        };
        _impl->get(on_get, wait_duration);
    }
};

} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
