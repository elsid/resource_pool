#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

template <class Value, class IoService, class Queue>
class pool_impl : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef boost::shared_ptr<value_type> pointer;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef Queue queue_type;
    typedef typename queue_type::time_duration time_duration;

    pool_impl(io_service_t& io_service,
              std::size_t capacity,
              std::size_t queue_capacity)
            : _io_service(io_service),
              _capacity(assert_capacity(capacity)),
              _callbacks(boost::make_shared<queue_type>(boost::ref(io_service), queue_capacity)),
              _available_size(0),
              _used_size(0),
              _disabled(false)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    const queue_type& queue() const { return *_callbacks; }

    void async_call(const boost::function<void ()>& call) {
        _io_service.post(bind(call_and_abort_on_catch_exception, call));
    }

    void get(const callback& call, time_duration wait_duration = seconds(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    typedef boost::unique_lock<boost::mutex> unique_lock;
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef void (pool_impl::*serve_request_t)(unique_lock&, const callback&);

    mutable boost::mutex _mutex;
    list _available;
    list _used;
    io_service_t& _io_service;
    const std::size_t _capacity;
    boost::shared_ptr<queue_type> _callbacks;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    void reserve_resource(unique_lock& lock, const callback& call);
    void alloc_resource(unique_lock& lock, const callback& call);
    void perform_one_request(unique_lock& lock, serve_request_t serve);
    static void call_and_abort_on_catch_exception(const boost::function<void ()>& call) throw();
};

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class V, class I, class Q>
std::size_t pool_impl<V, I, Q>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::recycle(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    perform_one_request(lock, &pool_impl::alloc_resource);
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    perform_one_request(lock, &pool_impl::reserve_resource);
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::get(const callback& call, time_duration wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        async_call(bind(call, make_error_code(error::disabled), list_iterator()));
    } else if (!_available.empty()) {
        alloc_resource(lock, call);
    } else if (fit_capacity()) {
        reserve_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0) {
            async_call(bind(call, make_error_code(error::get_resource_timeout),
                            list_iterator()));
        } else {
            const boost::function<void ()> expired = bind(call,
                make_error_code(error::get_resource_timeout), list_iterator());
            const bool pushed = _callbacks->push(call,
                bind(call_and_abort_on_catch_exception, expired), wait_duration);
            if (!pushed) {
                async_call(bind(call, make_error_code(error::request_queue_overflow),
                                list_iterator()));
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
        async_call(bind(call, make_error_code(error::disabled),
                        list_iterator()));
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
void pool_impl<V, I, Q>::alloc_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::reserve_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _used.insert(_used.end(), pointer());
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::perform_one_request(unique_lock& lock, serve_request_t serve) {
    callback call;
    if (_callbacks->pop(call)) {
        (this->*serve)(lock, call);
    }
}

template <class V, class I, class Q>
void pool_impl<V, I, Q>::call_and_abort_on_catch_exception(const boost::function<void ()>& call) throw() {
    try {
        call();
    } catch (...) {
        assert(false && "resource_pool callbacks must not throw exceptions");
        abort();
    }
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
