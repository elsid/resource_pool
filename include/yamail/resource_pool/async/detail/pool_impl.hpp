#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <set>

#include <boost/optional.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/detail/request_queue/queue.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

template <
    class Resource,
    class ResourceCompare = std::less<Resource>,
    class ResourceAlloc = std::allocator<Resource> >
class pool_impl : boost::noncopyable,
    public boost::enable_shared_from_this<pool_impl<Resource, ResourceCompare, ResourceAlloc> > {
public:
    typedef Resource resource;
    typedef ResourceCompare resource_compare;
    typedef ResourceAlloc resource_alloc;
    typedef boost::shared_ptr<pool_impl> shared_ptr;
    typedef boost::optional<resource> resource_opt;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<void (resource)> make_resource_callback_succeed;
    typedef boost::function<void ()> make_resource_callback_failed;
    typedef boost::function<void (make_resource_callback_succeed,
        make_resource_callback_failed)> make_resource;
    typedef boost::function<void (const error::code&, const resource_opt&)> callback;
    typedef detail::request_queue::queue<callback> callback_queue;
    typedef typename callback_queue::time_duration time_duration;
    typedef boost::asio::io_service io_service;

    pool_impl(io_service& io_service, std::size_t capacity,
            std::size_t queue_capacity, make_resource make_res)
            : _io_service(io_service),
              _callbacks(boost::make_shared<callback_queue>(
                boost::ref(io_service), queue_capacity)),
              _capacity(capacity),
              _make_resource(make_res),
              _creating(0)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    std::size_t creating() const;
    std::size_t queue_capacity() const { return _callbacks->capacity(); }
    std::size_t queue_size() const { return _callbacks->size(); }
    bool queue_empty() const { return _callbacks->empty(); }
    void async_call(boost::function<void ()> call) { _io_service.post(call); }

    shared_ptr shared_from_this() {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }

    void get(callback call, const time_duration& wait_duration = seconds(0));
    void recycle(resource res);
    void waste(resource res);

private:
    typedef typename callback_queue::shared_ptr callback_queue_ptr;
    typedef std::set<resource, resource_compare, resource_alloc> resource_set;
    typedef typename resource_set::iterator resource_set_iterator;
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    resource_set _available;
    resource_set _used;
    io_service& _io_service;
    callback_queue_ptr _callbacks;
    const std::size_t _capacity;
    make_resource _make_resource;
    std::size_t _creating;

    std::size_t size_unsafe() const { return _available.size() + _used.size(); }
    bool fit_capacity() const { return size_unsafe() + _creating < _capacity; }
    void async_create(unique_lock& lock);
    void remove_used(const resource& res);
    void alloc_resource(unique_lock& lock, const callback& call);
    void perform_one_request(unique_lock& lock);
    void add_new(const resource& res);
    void retry_create();
};

template <class R, class C, class A>
std::size_t pool_impl<R, C, A>::size() const {
    const unique_lock lock(_mutex);
    return size_unsafe();
}

template <class R, class C, class A>
std::size_t pool_impl<R, C, A>::available() const {
    const unique_lock lock(_mutex);
    return _available.size();
}

template <class R, class C, class A>
std::size_t pool_impl<R, C, A>::used() const {
    const unique_lock lock(_mutex);
    return _used.size();
}

template <class R, class C, class A>
std::size_t pool_impl<R, C, A>::creating() const {
    const unique_lock lock(_mutex);
    return _creating;
}

template <class R, class C, class A>
void pool_impl<R, C, A>::recycle(resource res) {
    unique_lock lock(_mutex);
    remove_used(res);
    _available.insert(res);
    perform_one_request(lock);
}

template <class R, class C, class A>
void pool_impl<R, C, A>::waste(resource res) {
    unique_lock lock(_mutex);
    remove_used(res);
    if (_callbacks->empty()) {
        return;
    }
    if (_available.empty()) {
        async_create(lock);
    } else {
        perform_one_request(lock);
    }
}

template <class R, class C, class A>
void pool_impl<R, C, A>::get(callback call, const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (!_available.empty()) {
        alloc_resource(lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0ll) {
            async_call(bind(call, error::get_resource_timeout, resource_opt()));
        } else {
            const error::code& push_result = _callbacks->push(call,
                bind(call, error::get_resource_timeout, resource_opt()),
                wait_duration);
            if (push_result == error::none) {
                unique_lock lock(_mutex);
                if (fit_capacity()) {
                    async_create(lock);
                }
            } else {
                async_call(bind(call, push_result, resource_opt()));
            }
        }
    }
}

template <class R, class C, class A>
void pool_impl<R, C, A>::alloc_resource(unique_lock& lock, const callback& call) {
    const resource_set_iterator available = _available.begin();
    const resource_set_iterator used = _used.insert(*available).first;
    _available.erase(available);
    const resource res = *used;
    lock.unlock();
    async_call(bind(call, error::none, res));
}

template <class R, class C, class A>
void pool_impl<R, C, A>::async_create(unique_lock& lock) {
    ++_creating;
    lock.unlock();
    const make_resource_callback_succeed succeed = _io_service.wrap(
        bind(&pool_impl::add_new, shared_from_this(), _1));
    const make_resource_callback_failed failed = _io_service.wrap(
        bind(&pool_impl::retry_create, shared_from_this()));
    async_call(bind(_make_resource, succeed, failed));
}

template <class R, class C, class A>
void pool_impl<R, C, A>::remove_used(const resource& res) {
    const resource_set_iterator it = _used.find(res);
    if (it == _used.end()) {
        if (_available.find(res) == _available.end()) {
            throw error::resource_not_from_pool();
        } else {
            throw error::add_existing_resource();
        }
    }
    _used.erase(it);
}

template <class R, class C, class A>
void pool_impl<R, C, A>::perform_one_request(unique_lock& lock) {
    const typename callback_queue::pop_result& result = _callbacks->pop();
    if (result == error::none) {
        alloc_resource(lock, *result.request);
    }
}

template <class R, class C, class A>
void pool_impl<R, C, A>::add_new(const resource& res) {
    unique_lock lock(_mutex);
    --_creating;
    if (!_available.insert(res).second) {
        throw error::add_existing_resource();
    }
    perform_one_request(lock);
}

template <class R, class C, class A>
void pool_impl<R, C, A>::retry_create() {
    unique_lock lock(_mutex);
    --_creating;
    if (fit_capacity() && !queue_empty()) {
        async_create(lock);
    }
}

}}}}

#endif
