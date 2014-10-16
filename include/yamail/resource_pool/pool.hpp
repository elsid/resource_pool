#ifndef YAMAIL_RESOURCE_POOL_POOL_HPP
#define YAMAIL_RESOURCE_POOL_POOL_HPP

#include <set>
#include <vector>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>

namespace yamail { namespace resource_pool {

template <class T>
struct object_factory {
    T operator()() { return T(); }
};

template <class Resource>
class pool : public boost::enable_shared_from_this<pool<Resource> >,
    boost::noncopyable {
public:
    typedef Resource resource;
    typedef pool<resource> this_type;
    typedef boost::shared_ptr<this_type> shared_ptr;
    typedef boost::shared_ptr<const this_type> shared_const_ptr;
    typedef boost::posix_time::time_duration duration;
    typedef boost::posix_time::seconds seconds;
    typedef object_factory<resource> resource_factory;
    typedef boost::function<resource ()> make_resource;
    typedef resource_pool::handle<this_type> handle;
    typedef boost::shared_ptr<handle> handle_ptr;
    typedef boost::optional<resource> resource_opt;
    typedef void (handle::*strategy)();

    pool(std::size_t capacity = 0ul, make_resource make_res = resource_factory())
            : _capacity(capacity), _make_resource(make_res)
    {}

    shared_ptr shared_from_this() {
        return boost::enable_shared_from_this<this_type>::shared_from_this();
    }
    shared_const_ptr shared_from_this() const {
        return boost::enable_shared_from_this<this_type>::shared_from_this();
    }

    std::size_t capacity() const;
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    void fill();
    void clear();

    handle_ptr get_auto_waste(const duration& wait_duration = seconds(0));
    handle_ptr get_auto_recycle(const duration& wait_duration = seconds(0));
    resource_opt get(const duration& wait_duration = seconds(0));
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
    handle_ptr get_handle(strategy use_use_strategy, const duration& wait_duration);
};

template <class R>
std::size_t pool<R>::capacity() const {
    unique_lock lock(_mutex);
    return _capacity;
}

template <class R>
std::size_t pool<R>::size() const {
    unique_lock lock(_mutex);
    return size_unsafe();
}

template <class R>
std::size_t pool<R>::available() const {
    unique_lock lock(_mutex);
    return _available.size();
}

template <class R>
std::size_t pool<R>::used() const {
    unique_lock lock(_mutex);
    return _used.size();
}

template <class R>
void pool<R>::fill() {
    unique_lock lock(_mutex);
    while (fit_capacity()) {
        create();
        _has_available.notify_one();
    }
}

template <class R>
void pool<R>::clear() {
    unique_lock lock(_mutex);
    _available.clear();
}

template <class R>
void pool<R>::recycle(resource resource) {
    unique_lock lock(_mutex);
    remove_used(resource);
    add_available(resource);
    _has_available.notify_one();
}

template <class R>
void pool<R>::waste(resource resource) {
    unique_lock lock(_mutex);
    remove_used(resource);
    _has_available.notify_one();
}

template <class R>
typename pool<R>::handle_ptr pool<R>::get_auto_waste(
        const duration& wait_duration) {
    return get_handle(&handle::waste, wait_duration);
}

template <class R>
typename pool<R>::handle_ptr pool<R>::get_auto_recycle(
        const duration& wait_duration) {
    return get_handle(&handle::recycle, wait_duration);
}

template <class R>
typename pool<R>::resource_opt pool<R>::get(const duration& wait_duration) {
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
typename pool<R>::resource_set_iterator pool<R>::add_available(const resource& res) {
    std::pair<resource_set_iterator, bool> inserted = _available.insert(res);
    if (!inserted.second) {
        throw add_existing_resource();
    }
    return inserted.first;
}

template <class R>
typename pool<R>::resource_set_iterator pool<R>::add_used(const resource& res) {
    std::pair<resource_set_iterator, bool> inserted = _used.insert(res);
    if (!inserted.second) {
        throw add_existing_resource();
    }
    return inserted.first;
}

template <class R>
void pool<R>::remove_used(const resource& resource) {
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

template <class R>
typename pool<R>::handle_ptr pool<R>::get_handle(strategy use_use_strategy,
        const duration& wait_duration) {
    return handle_ptr(new handle(shared_from_this(), use_use_strategy,
        wait_duration));
}

}}

#endif
