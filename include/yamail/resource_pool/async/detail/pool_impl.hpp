#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <list>

#include <boost/optional.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/detail/request_queue/queue.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

template <class T>
class pool_impl : boost::noncopyable,
    public boost::enable_shared_from_this<pool_impl<T> > {
public:
    typedef T value_type;
    typedef boost::shared_ptr<value_type> pointer;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef boost::optional<list_iterator> list_iterator_opt;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<void (const boost::system::error_code&,
        const list_iterator_opt&)> callback;
    typedef detail::request_queue::queue<callback> callback_queue;
    typedef typename callback_queue::time_duration time_duration;

    pool_impl(boost::asio::io_service& io_service, std::size_t capacity,
            std::size_t queue_capacity)
            : _io_service(io_service),
              _callbacks(boost::make_shared<callback_queue>(
                boost::ref(io_service), queue_capacity)),
              _capacity(capacity),
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
    void async_call(boost::function<void ()> call) { _io_service.post(call); }

    boost::shared_ptr<pool_impl> shared_from_this() {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }

    void get(callback call, const time_duration& wait_duration = seconds(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

private:
    typedef typename boost::shared_ptr<callback_queue> callback_queue_ptr;
    typedef boost::unique_lock<boost::mutex> unique_lock;
    typedef boost::lock_guard<boost::mutex> lock_guard;

    mutable boost::mutex _mutex;
    list _available;
    list _used;
    boost::asio::io_service& _io_service;
    callback_queue_ptr _callbacks;
    const std::size_t _capacity;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    void reserve_resource(unique_lock& lock, const callback& call);
    void alloc_resource(unique_lock& lock, const callback& call);
    void perform_one_request(unique_lock& lock);
};

template <class T>
std::size_t pool_impl<T>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class T>
std::size_t pool_impl<T>::available() const {
    const unique_lock lock(_mutex);
    return _available_size;
}

template <class T>
std::size_t pool_impl<T>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class T>
void pool_impl<T>::recycle(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    perform_one_request(lock);
}

template <class T>
void pool_impl<T>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    if (_callbacks->empty()) {
        return;
    }
    if (!_available.empty()) {
        perform_one_request(lock);
    }
}

template <class T>
void pool_impl<T>::get(callback call, const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        async_call(bind(call, make_error_code(error::disabled),
            list_iterator_opt()));
    } else if (!_available.empty()) {
        alloc_resource(lock, call);
    } else if (fit_capacity()) {
        reserve_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0ll) {
            async_call(bind(call, make_error_code(error::get_resource_timeout),
                list_iterator_opt()));
        } else {
            const boost::system::error_code& push_result = _callbacks->push(call,
                bind(call, make_error_code(error::get_resource_timeout),
                    list_iterator_opt()),
                wait_duration);
            if (push_result != boost::system::error_code()) {
                async_call(bind(call, push_result, list_iterator_opt()));
            }
        }
    }
}

template <class T>
void pool_impl<T>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    const boost::system::error_code& empty_queue = make_error_code(
        error::request_queue_is_empty);
    while (true) {
        const typename callback_queue::pop_result& result = _callbacks->pop();
        if (result == empty_queue) {
            break;
        } else if (result == boost::system::error_code()) {
            async_call(bind(*result.request,
                make_error_code(error::disabled),
                list_iterator_opt()));
        }
    }
}

template <class T>
void pool_impl<T>::alloc_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class T>
void pool_impl<T>::reserve_resource(unique_lock& lock, const callback& call) {
    const list_iterator res_it = _used.insert(_used.end(), pointer());
    ++_used_size;
    lock.unlock();
    async_call(bind(call, boost::system::error_code(), res_it));
}

template <class T>
void pool_impl<T>::perform_one_request(unique_lock& lock) {
    const typename callback_queue::pop_result& result = _callbacks->pop();
    if (result == boost::system::error_code()) {
        alloc_resource(lock, *result.request);
    }
}

}
}
}
}

#endif
