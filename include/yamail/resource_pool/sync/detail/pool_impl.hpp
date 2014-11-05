#ifndef YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP

#include <set>

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
    class ResourceCompare = std::less<Resource>,
    class ResourceAlloc = std::allocator<Resource> >
class pool_impl : boost::noncopyable {
public:
    typedef Resource resource;
    typedef ResourceCompare resource_compare;
    typedef ResourceAlloc resource_alloc;
    typedef boost::chrono::system_clock::duration time_duration;
    typedef boost::chrono::seconds seconds;
    typedef boost::function<resource ()> make_resource;
    typedef boost::optional<resource> resource_opt;
    typedef std::pair<error::code, resource_opt> get_result;

    pool_impl(std::size_t capacity, make_resource make_res)
            : _capacity(capacity), _make_resource(make_res)
    {}

    std::size_t capacity() const;
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    get_result get(const time_duration& wait_duration = seconds(0));
    void recycle(resource res);
    void waste(resource res);

private:
    typedef std::set<resource, resource_compare, resource_alloc> resource_set;
    typedef typename resource_set::iterator resource_set_iterator;
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    boost::condition_variable _has_available;
    resource_set _available;
    resource_set _used;
    std::size_t _capacity;
    make_resource _make_resource;

    std::size_t size_unsafe() const { return _available.size() + _used.size(); }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    void create() { add_available(_make_resource()); }
    resource_set_iterator add_available(const resource& resource);
    resource_set_iterator add_used(const resource& resource);
    void remove_used(const resource& resource);
    bool has_available() const { return !_available.empty(); }
    bool create_if_can();
    bool wait_for(unique_lock& lock, const time_duration& wait_duration);
};

template <class R, class C, class A>
std::size_t pool_impl<R, C, A>::capacity() const {
    unique_lock lock(_mutex);
    return _capacity;
}

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
void pool_impl<R, C, A>::recycle(resource resource) {
    const unique_lock lock(_mutex);
    remove_used(resource);
    add_available(resource);
    _has_available.notify_one();
}

template <class R, class C, class A>
void pool_impl<R, C, A>::waste(resource resource) {
    const unique_lock lock(_mutex);
    remove_used(resource);
    _has_available.notify_one();
}

template <class R, class C, class A>
typename pool_impl<R, C, A>::get_result pool_impl<R, C, A>::get(
        const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (!wait_for(lock, wait_duration)) {
        return std::make_pair(error::get_resource_timeout, boost::none);
    }
    const resource_set_iterator available = _available.begin();
    const resource_set_iterator used = add_used(*available);
    _available.erase(available);
    return std::make_pair(error::none, *used);
}

template <class R, class C, class A>
typename pool_impl<R, C, A>::resource_set_iterator pool_impl<R, C, A>::add_available(
        const resource& res) {
    const std::pair<resource_set_iterator, bool> inserted = _available.insert(res);
    if (!inserted.second) {
        throw error::add_existing_resource();
    }
    return inserted.first;
}

template <class R, class C, class A>
typename pool_impl<R, C, A>::resource_set_iterator pool_impl<R, C, A>::add_used(
        const resource& res) {
    const std::pair<resource_set_iterator, bool> inserted = _used.insert(res);
    if (!inserted.second) {
        throw error::add_existing_resource();
    }
    return inserted.first;
}

template <class R, class C, class A>
void pool_impl<R, C, A>::remove_used(const resource& resource) {
    const resource_set_iterator it = _used.find(resource);
    if (it == _used.end()) {
        if (_available.find(resource) == _available.end()) {
            throw error::resource_not_from_pool();
        } else {
            throw error::add_existing_resource();
        }
    }
    _used.erase(it);
}

template <class R, class C, class A>
bool pool_impl<R, C, A>::create_if_can() {
    if (!fit_capacity()) {
        return false;
    }
    create();
    return true;
}

template <class R, class C, class A>
bool pool_impl<R, C, A>::wait_for(unique_lock& lock,
        const time_duration& wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this) or
        boost::bind(&pool_impl::create_if_can, this));
}

}}}}

#endif
