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

template <class T>
class pool_impl : boost::noncopyable {
public:
    typedef T value_type;
    typedef boost::shared_ptr<value_type> pointer;
    typedef boost::chrono::system_clock::duration time_duration;
    typedef boost::chrono::seconds seconds;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef std::pair<boost::system::error_code, list_iterator> get_result;

    pool_impl(std::size_t capacity)
            : _capacity(capacity),
              _available_size(0),
              _used_size(0),
              _disabled(false)
    {}

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;

    get_result get(const time_duration& wait_duration = seconds(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

private:
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef boost::unique_lock<boost::mutex> unique_lock;

    mutable boost::mutex _mutex;
    boost::condition_variable _has_available;
    list _available;
    list _used;
    const std::size_t _capacity;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    bool has_available() const { return !_available.empty(); }
    bool wait_for(unique_lock& lock, const time_duration& wait_duration);
};

template <class T>
std::size_t pool_impl<T>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class T>
std::size_t pool_impl<T>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class T>
std::size_t pool_impl<T>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class T>
void pool_impl<T>::recycle(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_available.notify_one();
}

template <class T>
void pool_impl<T>::waste(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    _has_available.notify_one();
}

template <class T>
void pool_impl<T>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    _has_available.notify_all();
}

template <class T>
typename pool_impl<T>::get_result pool_impl<T>::get(
        const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        return std::make_pair(make_error_code(error::disabled), list_iterator());
    }
    if (_available_size == 0 && fit_capacity()) {
        const list_iterator res_it = _used.insert(_used.end(), pointer());
        ++_used_size;
        return std::make_pair(boost::system::error_code(), res_it);
    }
    if (!wait_for(lock, wait_duration)) {
        return std::make_pair(make_error_code(error::get_resource_timeout),
            list_iterator());
    }
    if (_disabled) {
        return std::make_pair(make_error_code(error::disabled), list_iterator());
    }
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    return std::make_pair(boost::system::error_code(), res_it);
}

template <class T>
bool pool_impl<T>::wait_for(unique_lock& lock,
        const time_duration& wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this));
}

}
}
}
}

#endif
