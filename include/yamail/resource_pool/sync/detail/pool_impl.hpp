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
    typedef boost::optional<list_iterator> list_iterator_opt;
    typedef std::pair<boost::system::error_code, list_iterator_opt> get_result;

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
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    list_iterator add(pointer res);
    list_iterator replace(list_iterator res_it,
        pointer res);

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
    std::size_t _reserved;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() + _reserved < _capacity; }
    bool has_available() const { return !_available.empty(); }
    bool wait_for(unique_lock& lock, const time_duration& wait_duration);
};

template <class R>
std::size_t pool_impl<R>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class R>
std::size_t pool_impl<R>::available() const {
    const lock_guard lock(_mutex);
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
void pool_impl<R>::recycle(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_available.notify_one();
}

template <class R>
void pool_impl<R>::waste(list_iterator res_it) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    --_used_size;
    _has_available.notify_one();
}

template <class R>
typename pool_impl<R>::get_result pool_impl<R>::get(
        const time_duration& wait_duration) {
    unique_lock lock(_mutex);
    if (_available_size == 0 && fit_capacity()) {
        ++_reserved;
        return std::make_pair(boost::system::error_code(), boost::none);
    }
    if (!wait_for(lock, wait_duration)) {
        return std::make_pair(make_error_code(error::get_resource_timeout),
            boost::none);
    }
    const list_iterator res_it = _available.begin();
    _available.splice(_used.end(), _used, res_it);
    --_available_size;
    ++_used_size;
    return std::make_pair(boost::system::error_code(), res_it);
}

template <class R>
typename pool_impl<R>::list_iterator pool_impl<R>::add(pointer res) {
    const lock_guard lock(_mutex);
    if (_reserved == 0) {
        if (!fit_capacity()) {
            throw error::pool_overflow();
        }
    } else {
        --_reserved;
    }
    const list_iterator res_it = _used.insert(_used.end(), res);
    ++_used_size;
    return res_it;
}

template <class R>
typename pool_impl<R>::list_iterator pool_impl<R>::replace(
        list_iterator res_it, pointer res) {
    const lock_guard lock(_mutex);
    _used.erase(res_it);
    const list_iterator it = _used.insert(_used.end(), res);
    return it;
}

template <class R>
bool pool_impl<R>::wait_for(unique_lock& lock,
        const time_duration& wait_duration) {
    return _has_available.wait_for(lock, wait_duration,
        boost::bind(&pool_impl::has_available, this));
}

}
}
}
}

#endif
