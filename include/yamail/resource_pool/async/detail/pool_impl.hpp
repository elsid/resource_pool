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

template <class Resource>
class pool_impl : boost::noncopyable,
    public boost::enable_shared_from_this<pool_impl<Resource> > {
public:
    typedef Resource resource;
    typedef boost::shared_ptr<resource> resource_ptr;
    typedef boost::shared_ptr<pool_impl> shared_ptr;
    typedef std::list<resource_ptr> resource_list;
    typedef typename resource_list::iterator resource_ptr_list_iterator;
    typedef boost::optional<resource_ptr_list_iterator> resource_ptr_list_iterator_opt;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<void (const error::code&,
        const resource_ptr_list_iterator_opt&)> callback;
    typedef detail::request_queue::queue<callback> callback_queue;
    typedef typename callback_queue::time_duration time_duration;
    typedef boost::asio::io_service io_service;

    pool_impl(io_service& io_service, std::size_t capacity,
            std::size_t queue_capacity)
            : _io_service(io_service),
              _callbacks(boost::make_shared<callback_queue>(
                boost::ref(io_service), queue_capacity)),
              _capacity(capacity),
              _reserved(0),
              _available_size(0),
              _used_size(0)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    std::size_t reserved() const;
    std::size_t queue_capacity() const { return _callbacks->capacity(); }
    std::size_t queue_size() const { return _callbacks->size(); }
    bool queue_empty() const { return _callbacks->empty(); }
    void async_call(boost::function<void ()> call) { _io_service.post(call); }

    shared_ptr shared_from_this() {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }

    void get(callback call, const time_duration& wait_duration = seconds(0));
    void recycle(resource_ptr_list_iterator res_it);
    void waste(resource_ptr_list_iterator res_it);
    resource_ptr_list_iterator add(resource_ptr res);
    resource_ptr_list_iterator replace(resource_ptr_list_iterator res_it, resource_ptr res);

private:
    typedef typename callback_queue::shared_ptr callback_queue_ptr;
    typedef boost::unique_lock<boost::mutex> unique_lock;
    typedef boost::lock_guard<boost::mutex> lock_guard;

    mutable boost::mutex _mutex;
    resource_list _available;
    resource_list _used;
    io_service& _io_service;
    callback_queue_ptr _callbacks;
    const std::size_t _capacity;
    std::size_t _reserved;
    std::size_t _available_size;
    std::size_t _used_size;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() + _reserved < _capacity; }
    void reserve_resource(unique_lock& lock, const callback& call);
    void alloc_resource(unique_lock& lock, const callback& call);
    void perform_one_request(unique_lock& lock);
};

template <class R>
std::size_t pool_impl<R>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class R>
std::size_t pool_impl<R>::available() const {
    const unique_lock lock(_mutex);
    return _available_size;
}

template <class R>
std::size_t pool_impl<R>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class R>
std::size_t pool_impl<R>::reserved() const {
    const lock_guard lock(_mutex);
    return _reserved;
}

template <class R>
void pool_impl<R>::recycle(resource_ptr_list_iterator res_it) {
    unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    perform_one_request(lock);
}

template <class R>
void pool_impl<R>::waste(resource_ptr_list_iterator res_it) {
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

template <class R>
void pool_impl<R>::get(callback call, const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (!_available.empty()) {
        alloc_resource(lock, call);
    } else if (fit_capacity()) {
        reserve_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0ll) {
            async_call(bind(call, error::get_resource_timeout,
                resource_ptr_list_iterator_opt()));
        } else {
            const error::code& push_result = _callbacks->push(call,
                bind(call, error::get_resource_timeout, resource_ptr_list_iterator_opt()),
                wait_duration);
            if (push_result != error::none) {
                async_call(bind(call, push_result, resource_ptr_list_iterator_opt()));
            }
        }
    }
}

template <class R>
typename pool_impl<R>::resource_ptr_list_iterator pool_impl<R>::add(resource_ptr res) {
    const lock_guard lock(_mutex);
    if (_reserved == 0) {
        if (!fit_capacity()) {
            throw error::pool_overflow();
        }
    } else {
        --_reserved;
    }
    const resource_ptr_list_iterator res_it = _used.insert(_used.end(), res);
    ++_used_size;
    return res_it;
}

template <class R>
typename pool_impl<R>::resource_ptr_list_iterator pool_impl<R>::replace(
        resource_ptr_list_iterator res_it, resource_ptr res) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    const resource_ptr_list_iterator new_it = _used.insert(_used.end(), res);
    return new_it;
}

template <class R>
void pool_impl<R>::alloc_resource(unique_lock& lock, const callback& call) {
    const resource_ptr_list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    lock.unlock();
    async_call(bind(call, error::none, res_it));
}

template <class R>
void pool_impl<R>::reserve_resource(unique_lock& lock, const callback& call) {
    ++_reserved;
    lock.unlock();
    async_call(bind(call, error::none, boost::none));
}

template <class R>
void pool_impl<R>::perform_one_request(unique_lock& lock) {
    const typename callback_queue::pop_result& result = _callbacks->pop();
    if (result == error::none) {
        alloc_resource(lock, *result.request);
    }
}

}}}}

#endif
