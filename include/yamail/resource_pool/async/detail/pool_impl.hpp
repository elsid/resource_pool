#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

using detail::clock;

template <class Value,
          class IoService = boost::asio::io_service,
          class Timer = boost::asio::basic_waitable_timer<clock> >
class pool_impl : boost::noncopyable,
    public boost::enable_shared_from_this<pool_impl<Value, IoService, Timer> > {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Timer timer_t;
    typedef boost::shared_ptr<value_type> pointer;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef detail::queue<callback, io_service_t, timer_t> callback_queue;
    typedef typename callback_queue::time_duration time_duration;

    pool_impl(io_service_t& io_service,
              const boost::shared_ptr<timer_t>& timer,
              std::size_t capacity,
              std::size_t queue_capacity)
            : _io_service(io_service),
              _capacity(assert_capacity(capacity)),
              _callbacks(boost::make_shared<callback_queue>(
                boost::ref(io_service), timer, queue_capacity)),
              _available_size(0),
              _used_size(0),
              _disabled(false)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    std::size_t queue_capacity() const { return _callbacks->capacity(); }
    std::size_t queue_size() const { return _callbacks->size(); }
    bool queue_empty() const { return _callbacks->empty(); }
    void async_call(const boost::function<void ()>& call) {
        _io_service.post(bind(call_and_abort_on_catch_exception, call));
    }

    boost::shared_ptr<pool_impl> shared_from_this() {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }

    void get(const callback& call, time_duration wait_duration = seconds(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    typedef typename boost::shared_ptr<callback_queue> callback_queue_ptr;
    typedef boost::unique_lock<boost::mutex> unique_lock;
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef void (pool_impl::*serve_request_t)(unique_lock&, const callback&);

    mutable boost::mutex _mutex;
    list _available;
    list _used;
    io_service_t& _io_service;
    const std::size_t _capacity;
    callback_queue_ptr _callbacks;
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

template <class V, class I, class T>
std::size_t pool_impl<V, I, T>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class V, class I, class T>
std::size_t pool_impl<V, I, T>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class V, class I, class T>
std::size_t pool_impl<V, I, T>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class V, class I, class T>
void pool_impl<V, I, T>::recycle(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    perform_one_request(lock, &pool_impl::alloc_resource);
}

template <class V, class I, class T>
void pool_impl<V, I, T>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    perform_one_request(lock, &pool_impl::reserve_resource);
}

template <class V, class I, class T>
void pool_impl<V, I, T>::get(const callback& call, time_duration wait_duration) {
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
        if (wait_duration.count() == 0ll) {
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

template <class V, class I, class T>
void pool_impl<V, I, T>::disable() {
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

template <class V, class I, class T>
std::size_t pool_impl<V, I, T>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

template <class V, class I, class T>
void pool_impl<V, I, T>::alloc_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class V, class I, class T>
void pool_impl<V, I, T>::reserve_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _used.insert(_used.end(), pointer());
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class V, class I, class T>
void pool_impl<V, I, T>::perform_one_request(unique_lock& lock, serve_request_t serve) {
    callback call;
    if (_callbacks->pop(call)) {
        (this->*serve)(lock, call);
    }
}

template <class V, class I, class T>
void pool_impl<V, I, T>::call_and_abort_on_catch_exception(const boost::function<void ()>& call) throw() {
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
