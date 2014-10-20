#ifndef YAMAIL_RESOURCE_POOL_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_DETAIL_POOL_IMPL_HPP

#include <set>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace detail {

template <class Resource>
class pool_impl : public boost::enable_shared_from_this<pool_impl<Resource> >,
    boost::noncopyable {
public:
    typedef Resource resource;
    typedef boost::shared_ptr<pool_impl> shared_ptr;
    typedef boost::shared_ptr<const pool_impl> shared_const_ptr;
    typedef boost::posix_time::time_duration time_duration;
    typedef boost::posix_time::seconds seconds;
    typedef boost::function<resource ()> make_resource;
    typedef boost::optional<resource> resource_opt;

    pool_impl(std::size_t capacity, make_resource make_res)
            : _capacity(capacity), _make_resource(make_res)
    {}

    shared_ptr shared_from_this() {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }
    shared_const_ptr shared_from_this() const {
        return boost::enable_shared_from_this<pool_impl>::shared_from_this();
    }

    std::size_t capacity() const;
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    void fill();
    void clear();

    resource_opt get(const time_duration& wait_duration = seconds(0));
    void recycle(resource res);
    void waste(resource res);

private:
    typedef std::set<resource> resource_set;
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
};

template <class R>
std::size_t pool_impl<R>::capacity() const {
    unique_lock lock(_mutex);
    return _capacity;
}

template <class R>
std::size_t pool_impl<R>::size() const {
    unique_lock lock(_mutex);
    return size_unsafe();
}

template <class R>
std::size_t pool_impl<R>::available() const {
    unique_lock lock(_mutex);
    return _available.size();
}

template <class R>
std::size_t pool_impl<R>::used() const {
    unique_lock lock(_mutex);
    return _used.size();
}

template <class R>
void pool_impl<R>::fill() {
    unique_lock lock(_mutex);
    while (fit_capacity()) {
        create();
        _has_available.notify_one();
    }
}

template <class R>
void pool_impl<R>::clear() {
    unique_lock lock(_mutex);
    _available.clear();
}

template <class R>
void pool_impl<R>::recycle(resource resource) {
    unique_lock lock(_mutex);
    remove_used(resource);
    add_available(resource);
    _has_available.notify_one();
}

template <class R>
void pool_impl<R>::waste(resource resource) {
    unique_lock lock(_mutex);
    remove_used(resource);
    _has_available.notify_one();
}

template <class R>
typename pool_impl<R>::resource_opt pool_impl<R>::get(const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    while (_available.empty()) {
        if (fit_capacity()) {
            create();
            break;
        }
        if (!_has_available.timed_wait(lock, wait_duration)) {
            return resource_opt();
        }
    }
    resource_set_iterator available = _available.begin();
    resource_set_iterator used = add_used(*available);
    _available.erase(available);
    return *used;
}

template <class R>
typename pool_impl<R>::resource_set_iterator pool_impl<R>::add_available(const resource& res) {
    std::pair<resource_set_iterator, bool> inserted = _available.insert(res);
    if (!inserted.second) {
        throw add_existing_resource();
    }
    return inserted.first;
}

template <class R>
typename pool_impl<R>::resource_set_iterator pool_impl<R>::add_used(const resource& res) {
    std::pair<resource_set_iterator, bool> inserted = _used.insert(res);
    if (!inserted.second) {
        throw add_existing_resource();
    }
    return inserted.first;
}

template <class R>
void pool_impl<R>::remove_used(const resource& resource) {
    resource_set_iterator it = _used.find(resource);
    if (it == _used.end()) {
        if (_available.find(resource) == _available.end()) {
            throw resource_not_from_pool();
        } else {
            throw add_existing_resource();
        }
    }
    _used.erase(it);
}

}}}

#endif
