#ifndef YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP

#include <list>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {
namespace detail {

template <
    class Resource,
    class ResourceAlloc = std::allocator<Resource> >
class pool_impl : boost::noncopyable {
public:
    typedef Resource resource;
    typedef ResourceAlloc resource_alloc;
    typedef boost::chrono::system_clock::duration time_duration;
    typedef boost::chrono::seconds seconds;
    typedef std::list<resource, resource_alloc> resource_list;
    typedef typename resource_list::iterator resource_list_iterator;
    typedef boost::optional<resource_list_iterator> resource_list_iterator_opt;
    typedef std::pair<error::code, resource_list_iterator_opt> get_result;

    pool_impl(std::size_t capacity)
            : _capacity(capacity),
              _available_size(0),
              _used_size(0),
              _reserved(0)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    std::size_t reserved() const;

    get_result get(const time_duration& wait_duration = seconds(0));
    void recycle(resource_list_iterator res_it);
    void waste(resource_list_iterator res_it);
    resource_list_iterator replace(resource_list_iterator res_it, resource res);
    resource_list_iterator add(resource res);

private:
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    boost::condition_variable _has_available;
    resource_list _available;
    resource_list _used;
    const std::size_t _capacity;
    std::size_t _available_size;
    std::size_t _used_size;
    std::size_t _reserved;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() + _reserved < _capacity; }
    bool has_available() const { return !_available.empty(); }
    bool wait_for(unique_lock& lock, const time_duration& wait_duration);
};

template <class R, class A>
std::size_t pool_impl<R, A>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class R, class A>
std::size_t pool_impl<R, A>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class R, class A>
std::size_t pool_impl<R, A>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class R, class A>
std::size_t pool_impl<R, A>::reserved() const {
    const lock_guard lock(_mutex);
    return _reserved;
}

template <class R, class A>
void pool_impl<R, A>::recycle(resource_list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_available.notify_one();
}

template <class R, class A>
void pool_impl<R, A>::waste(resource_list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    _has_available.notify_one();
}

template <class R, class A>
typename pool_impl<R, A>::get_result pool_impl<R, A>::get(
        const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (_available_size == 0 && fit_capacity()) {
        ++_reserved;
        return std::make_pair(error::none, boost::none);
    }
    if (!wait_for(lock, wait_duration)) {
        return std::make_pair(error::get_resource_timeout, boost::none);
    }
    const resource_list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    return std::make_pair(error::none, res_it);
}

template <class R, class A>
typename pool_impl<R, A>::resource_list_iterator pool_impl<R, A>::add(resource res) {
    const lock_guard lock(_mutex);
    if (_reserved == 0) {
        if (!fit_capacity()) {
            throw error::pool_overflow();
        }
    } else {
        --_reserved;
    }
    const resource_list_iterator res_it = _used.insert(_used.end(), res);
    ++_used_size;
    return res_it;
}

template <class R, class A>
typename pool_impl<R, A>::resource_list_iterator pool_impl<R, A>::replace(
        resource_list_iterator res_it, resource res) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    const resource_list_iterator it = _used.insert(_used.end(), res);
    return it;
}

template <class R, class A>
bool pool_impl<R, A>::wait_for(unique_lock& lock,
        const time_duration& wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this));
}

}}}}

#endif
