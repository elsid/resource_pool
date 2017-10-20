#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>
#include <boost/asio/spawn.hpp>
#include <type_traits>

namespace yamail {
namespace resource_pool {
namespace async {

struct stats {
    std::size_t size;
    std::size_t available;
    std::size_t used;
    std::size_t queue_size;
};

namespace detail {

template <class Value, class IoService, class Queue>
class pool_impl : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef resource_pool::detail::idle<value_type> idle;
    typedef std::list<idle> list;
    typedef typename list::iterator list_iterator;
    typedef std::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef Queue queue_type;

    pool_impl(io_service_t& io_service,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : _io_service(io_service),
              _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _callbacks(std::make_shared<queue_type>(_io_service, queue_capacity)) {

        for (std::size_t i = 0; i < _capacity; ++i) {
            _wasted.emplace_back(idle());
        }
    }

    template <class Generator>
    pool_impl(io_service_t& io_service,
              Generator&& gen_value,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : _io_service(io_service),
              _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _callbacks(std::make_shared<queue_type>(_io_service, queue_capacity)) {

        const auto drop_time = time_traits::add(time_traits::now(), _idle_timeout);
        for (std::size_t i = 0; i < _capacity; ++i) {
            _available.emplace_back(gen_value(), drop_time);
        }
    }

    template <class Iter>
    pool_impl(io_service_t& io_service,
              Iter first, Iter last,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : pool_impl(io_service,
                    [&]{ return std::move(*first++); },
                    std::distance(first, last),
                    queue_capacity,
                    idle_timeout) {
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    async::stats stats() const;

    const queue_type& queue() const { return *_callbacks; }

    template <class Callback>
    void async_call(Callback&& call) {
        _io_service.post(std::forward<Callback>(call));
    }

    template <class Callback>
    void get(Callback call, time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    typedef std::unique_lock<std::mutex> unique_lock;
    typedef std::lock_guard<std::mutex> lock_guard;
    typedef bool (pool_impl::*serve_request_t)(unique_lock&, const callback&);

    mutable std::mutex _mutex;
    list _available;
    list _used;
    list _wasted;
    io_service_t& _io_service;
    const std::size_t _capacity;
    const time_traits::duration _idle_timeout;
    std::shared_ptr<queue_type> _callbacks;
    bool _disabled = false;

    std::size_t size_unsafe() const { return _available.size() + _used.size(); }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    template <class Callback>
    bool reserve_resource(unique_lock& lock, Callback call);
    template <class Callback>
    bool alloc_resource(unique_lock& lock, Callback call);
    void perform_one_request(unique_lock& lock, serve_request_t serve);
};

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::available() const {
    const lock_guard lock(_mutex);
    return _available.size();
}

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::used() const {
    const lock_guard lock(_mutex);
    return _used.size();
}

template <class V, class I, class Q>
async::stats pool_impl<V, I, Q>::stats() const {
    const lock_guard lock(_mutex);
    return async::stats {size_unsafe(), _available.size(), _used.size(), _callbacks->size()};
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::recycle(list_iterator res_it) {
    res_it->drop_time = time_traits::add(time_traits::now(), _idle_timeout);
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    perform_one_request(lock, &pool_impl::alloc_resource);
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::waste(list_iterator res_it) {
    res_it->value.reset();
    unique_lock lock(_mutex);
    _used.splice(_wasted.end(), _wasted, res_it);
    perform_one_request(lock, &pool_impl::reserve_resource);
}

template <class V, class I, class Q>
template <class Callback>
void pool_impl<V, I, Q>::get(Callback call, time_traits::duration wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        async_call([call] () mutable {
            call(make_error_code(error::disabled), list_iterator());
        });
    } else if (alloc_resource(lock, call)) {
        return;
    } else if (fit_capacity()) {
        reserve_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0) {
            async_call([call] () mutable {
                call(make_error_code(error::get_resource_timeout), list_iterator());
            });
        } else {
            const auto expired = [call] () mutable {
                call(make_error_code(error::get_resource_timeout), list_iterator());
            };
            const bool pushed = _callbacks->push(call, expired, wait_duration);
            if (!pushed) {
                async_call([call] () mutable {
                    call(make_error_code(error::request_queue_overflow), list_iterator());
                });
            }
        }
    }
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    while (true) {
        callback call;
        if (!_callbacks->pop(call)) {
            break;
        }
        async_call([call] {
            call(make_error_code(error::disabled), list_iterator());
        });
    }
}

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

template <class V, class I, class Q>
template <class Callback>
bool pool_impl<V, I, Q>::alloc_resource(unique_lock& lock, Callback call) {
    while (!_available.empty()) {
        const list_iterator res_it = _available.begin();
        if (res_it->drop_time <= time_traits::now()) {
            res_it->value.reset();
            _available.splice(_wasted.end(), _wasted, res_it);
            continue;
        }
        _available.splice(_used.end(), _used, res_it);
        lock.unlock();
        async_call([call, res_it] () mutable {
            call(boost::system::error_code(), res_it);
        });
        return true;
    }
    return false;
}

template <class V, class I, class Q>
template <class Callback>
bool pool_impl<V, I, Q>::reserve_resource(unique_lock& lock, Callback call) {
    const list_iterator res_it = _wasted.begin();
    _wasted.splice(_used.end(), _used, res_it);
    lock.unlock();
    async_call([call, res_it] () mutable {
        call(boost::system::error_code(), res_it);
    });
    return true;
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::perform_one_request(unique_lock& lock, serve_request_t serve) {
    callback call;
    if (_callbacks->pop(call)) {
        if (!(this->*serve)(lock, call)) {
            reserve_resource(lock, call);
        }
    }
}

// Helper template to deduce the true type of a handler, capture a local copy
// of the handler, and then create an async_result for the handler.
template <typename Handler, typename Signature>
struct async_result_init
{
    explicit async_result_init(Handler orig_handler)
    : handler(std::move(orig_handler)),
      result(handler)
    {
    }

    using handler_type = typename ::boost::asio::handler_type<Handler, Signature>::type;
    handler_type handler;
    ::boost::asio::async_result<handler_type> result;
};

template <typename Handler, typename Signature>
using async_return_type = typename ::boost::asio::async_result<
      typename ::boost::asio::handler_type<Handler, Signature>::type
    >::type;

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
