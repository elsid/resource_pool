#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

class abort {
public:
    typedef void (*impl_type)();

    abort(impl_type impl = std::abort) : impl_(impl) {}

    void operator ()(const boost::system::error_code& ec) const throw() {
        std::cerr << "yamail::resource_pool::async::detail::pool_impl fatal error: " << ec.message() << std::endl;
        impl_();
    }

private:
    impl_type impl_;
};

template <class Value, class IoService, class OnCatchHandlerException, class Queue>
class pool_impl : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef boost::shared_ptr<value_type> pointer;
    typedef resource_pool::detail::idle<pointer> idle;
    typedef std::list<idle> list;
    typedef typename list::iterator list_iterator;
    typedef boost::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef Queue queue_type;
    typedef OnCatchHandlerException on_catch_handler_exception_type;

    pool_impl(io_service_t& io_service,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout,
              const on_catch_handler_exception_type& on_catch_handler_exception)
            : _io_service(io_service),
              _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _callbacks(boost::make_shared<queue_type>(boost::ref(io_service), queue_capacity)),
              _on_catch_handler_exception(on_catch_handler_exception),
              _available_size(0),
              _used_size(0),
              _disabled(false)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    const queue_type& queue() const { return *_callbacks; }

    template <class Callback>
    void async_call(const Callback& call) {
        _io_service.post(call_and_catch_exception<Callback>(call, _on_catch_handler_exception));
    }

    template <class Callback>
    void get(const Callback& call, time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    typedef boost::unique_lock<boost::mutex> unique_lock;
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef bool (pool_impl::*serve_request_t)(unique_lock&, const callback&);

    template <class Callback>
    struct async_callback {
        Callback call;
        const boost::system::error_code ec;
        const list_iterator it;

        async_callback(const Callback& call, const boost::system::error_code& ec, list_iterator it)
            : call(call), ec(ec), it(it) {}

        void operator ()() {
            call(ec, it);
        }
    };

    template <class Callback>
    struct call_and_catch_exception {
        Callback call;
        on_catch_handler_exception_type on_catch_handler_exception;

        call_and_catch_exception(const Callback& call,
                                 const on_catch_handler_exception_type& on_catch_handler_exception)
            : call(call), on_catch_handler_exception(on_catch_handler_exception) {}

        void operator ()() {
            try {
                call();
            } catch (...) {
                on_catch_handler_exception(make_error_code(error::client_handler_exception));
            }
        }
    };

    mutable boost::mutex _mutex;
    list _available;
    list _used;
    io_service_t& _io_service;
    const std::size_t _capacity;
    const time_traits::duration _idle_timeout;
    boost::shared_ptr<queue_type> _callbacks;
    on_catch_handler_exception_type _on_catch_handler_exception;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    bool reserve_resource(unique_lock& lock, const callback& call);
    bool alloc_resource(unique_lock& lock, const callback& call);
    void perform_one_request(unique_lock& lock, serve_request_t serve);
};

template <class V, class I, class O, class Q>
std::size_t pool_impl<V, I, O, Q>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class V, class I, class O, class Q>
std::size_t pool_impl<V, I, O, Q>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class V, class I, class O, class Q>
std::size_t pool_impl<V, I, O, Q>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class V, class I, class O, class Q>
void pool_impl<V, I, O, Q>::recycle(list_iterator res_it) {
    res_it->drop_time = time_traits::add(time_traits::now(), _idle_timeout);
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    perform_one_request(lock, &pool_impl::alloc_resource);
}

template <class V, class I, class O, class Q>
void pool_impl<V, I, O, Q>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    perform_one_request(lock, &pool_impl::reserve_resource);
}

template <class V, class I, class O, class Q>
template <class Callback>
void pool_impl<V, I, O, Q>::get(const Callback& call, time_traits::duration wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        async_call(async_callback<Callback>(call, make_error_code(error::disabled), list_iterator()));
    } else if (alloc_resource(lock, call)) {
        return;
    } else if (fit_capacity()) {
        reserve_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0) {
            async_call(async_callback<Callback>(call, make_error_code(error::get_resource_timeout),
                                                list_iterator()));
        } else {
            const async_callback<Callback> expired(call,
                make_error_code(error::get_resource_timeout), list_iterator());
            typedef call_and_catch_exception<async_callback<Callback> > expired_callback;
            const bool pushed = _callbacks->push(call, expired_callback(expired, _on_catch_handler_exception),
                                                 wait_duration);
            if (!pushed) {
                async_call(async_callback<Callback>(call, make_error_code(error::request_queue_overflow),
                                                    list_iterator()));
            }
        }
    }
}

template <class V, class I, class O, class Q>
void pool_impl<V, I, O, Q>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    while (true) {
        callback call;
        if (!_callbacks->pop(call)) {
            break;
        }
        async_call(async_callback<callback>(call, make_error_code(error::disabled),
                                            list_iterator()));
    }
}

template <class V, class I, class O, class Q>
std::size_t pool_impl<V, I, O, Q>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

template <class V, class I, class O, class Q>
bool pool_impl<V, I, O, Q>::alloc_resource(unique_lock& lock, const callback& call) {
    while (!_available.empty()) {
        const list_iterator res_it = _available.begin();
        if (res_it->drop_time <= time_traits::now()) {
            _available.erase(res_it);
            --_available_size;
            continue;
        }
        _available.splice(_used.end(), _used, res_it);
        --_available_size;
        ++_used_size;
        lock.unlock();
        async_call(async_callback<callback>(call, boost::system::error_code(), res_it));
        return true;
    }
    return false;
}

template <class V, class I, class O, class Q>
bool pool_impl<V, I, O, Q>::reserve_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _used.insert(_used.end(), idle());
    ++_used_size;
    lock.unlock();
    async_call(async_callback<callback>(call, boost::system::error_code(), res_it));
    return true;
}

template <class V, class I, class O, class Q>
void pool_impl<V, I, O, Q>::perform_one_request(unique_lock& lock, serve_request_t serve) {
    callback call;
    if (_callbacks->pop(call)) {
        (this->*serve)(lock, call);
    }
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
