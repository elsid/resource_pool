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
    typedef boost::function<resource ()> make_resource;
    typedef std::list<resource, resource_alloc> resource_list;
    typedef typename resource_list::iterator resource_list_iterator;
    typedef boost::optional<resource_list_iterator> resource_list_iterator_opt;
    typedef std::pair<error::code, resource_list_iterator_opt> get_result;

    pool_impl(std::size_t capacity, make_resource make_res)
            : _capacity(capacity), _make_resource(make_res), _available_size(0),
              _used_size(0)
    {}

    std::size_t capacity() const;
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    get_result get(const time_duration& wait_duration = seconds(0));
    void recycle(resource_list_iterator res_it);
    void waste(resource_list_iterator res_it);

private:
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    boost::condition_variable _has_available;
    resource_list _available;
    resource_list _used;
    std::size_t _capacity;
    make_resource _make_resource;
    std::size_t _available_size;
    std::size_t _used_size;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    bool has_available() const { return !_available.empty(); }
    bool create_if_can();
    bool wait_for(unique_lock& lock, const time_duration& wait_duration);
};

template <class R, class A>
std::size_t pool_impl<R, A>::capacity() const {
    unique_lock lock(_mutex);
    return _capacity;
}

template <class R, class A>
std::size_t pool_impl<R, A>::size() const {
    const unique_lock lock(_mutex);
    return size_unsafe();
}

template <class R, class A>
std::size_t pool_impl<R, A>::available() const {
    const unique_lock lock(_mutex);
    return _available.size();
}

template <class R, class A>
std::size_t pool_impl<R, A>::used() const {
    const unique_lock lock(_mutex);
    return _used.size();
}

template <class R, class A>
void pool_impl<R, A>::recycle(resource_list_iterator res_it) {
    const unique_lock lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_available.notify_one();
}

template <class R, class A>
void pool_impl<R, A>::waste(resource_list_iterator res_it) {
    const unique_lock lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    _has_available.notify_one();
}

template <class R, class A>
typename pool_impl<R, A>::get_result pool_impl<R, A>::get(
        const time_duration& wait_duration) {
    unique_lock lock(_mutex);
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
bool pool_impl<R, A>::create_if_can() {
    if (!fit_capacity()) {
        return false;
    }
    _available.push_back(_make_resource());
    ++_available_size;
    return true;
}

template <class R, class A>
bool pool_impl<R, A>::wait_for(unique_lock& lock,
        const time_duration& wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this) or
        boost::bind(&pool_impl::create_if_can, this));
}

}}}}

#endif
