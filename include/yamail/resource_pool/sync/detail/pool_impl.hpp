#ifndef YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>

#include <condition_variable>
#include <list>
#include <mutex>

namespace yamail {
namespace resource_pool {
namespace sync {

struct stats {
    std::size_t size;
    std::size_t available;
    std::size_t used;
};

namespace detail {

template <class Value, class Mutex, class ConditionVariable>
class pool_impl {
public:
    using value_type = Value;
    using condition_variable = ConditionVariable;
    using idle = resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = typename list::iterator;
    using get_result = std::pair<boost::system::error_code, list_iterator>;

    pool_impl(std::size_t capacity, time_traits::duration idle_timeout)
            : _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _available_size(0),
              _used_size(0),
              _disabled(false) {
        for (std::size_t i = 0; i < _capacity; ++i) {
            _wasted.emplace_back(idle());
        }
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    sync::stats stats() const;

    const condition_variable& has_capacity() const { return _has_capacity; }

    get_result get(time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    using mutex_t = Mutex;
    using lock_guard = std::lock_guard<mutex_t>;
    using unique_lock = std::unique_lock<mutex_t>;

    mutable mutex_t _mutex;
    list _available;
    list _used;
    list _wasted;
    const std::size_t _capacity;
    const time_traits::duration _idle_timeout;
    condition_variable _has_capacity;
    std::size_t _available_size;
    std::size_t _used_size;
    bool _disabled;

    std::size_t size_unsafe() const { return _available_size + _used_size; }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    list_iterator alloc_resource(unique_lock& lock);
    list_iterator reserve_resource(unique_lock& lock);
    bool wait_for(unique_lock& lock, time_traits::duration wait_duration);
};

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::available() const {
    const lock_guard lock(_mutex);
    return _available_size;
}

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::used() const {
    const lock_guard lock(_mutex);
    return _used_size;
}

template <class T, class M, class C>
sync::stats pool_impl<T, M, C>::stats() const {
    const lock_guard lock(_mutex);
    return sync::stats {size_unsafe(), _available_size, _used_size};
}

template <class T, class M, class C>
void pool_impl<T, M, C>::recycle(list_iterator res_it) {
    res_it->drop_time = time_traits::add(time_traits::now(), _idle_timeout);
    const lock_guard lock(_mutex);
    _used.splice(_available.end(), _available, res_it);
    --_used_size;
    ++_available_size;
    _has_capacity.notify_one();
}

template <class T, class M, class C>
void pool_impl<T, M, C>::waste(list_iterator res_it) {
    res_it->value.reset();
    const lock_guard lock(_mutex);
    _used.splice(_wasted.end(), _wasted, res_it);
    --_used_size;
    _has_capacity.notify_one();
}

template <class T, class M, class C>
void pool_impl<T, M, C>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    _has_capacity.notify_all();
}

template <class T, class M, class C>
typename pool_impl<T, M, C>::get_result pool_impl<T, M, C>::get(time_traits::duration wait_duration) {
    unique_lock lock(_mutex);
    while (true) {
        if (_disabled) {
            lock.unlock();
            return std::make_pair(make_error_code(error::disabled), list_iterator());
        } else if (!_available.empty()) {
            const list_iterator res_it = alloc_resource(lock);
            if (res_it != list_iterator()) {
                return std::make_pair(boost::system::error_code(), res_it);
            }
        }
        if (fit_capacity()) {
            return std::make_pair(boost::system::error_code(), reserve_resource(lock));
        }
        if (!wait_for(lock, wait_duration)) {
            lock.unlock();
            return std::make_pair(make_error_code(error::get_resource_timeout),
                                  list_iterator());
        }
    }
}

template <class T, class M, class C>
typename pool_impl<T, M, C>::list_iterator pool_impl<T, M, C>::alloc_resource(unique_lock& lock) {
    while (!_available.empty()) {
        const list_iterator res_it = _available.begin();
        if (res_it->drop_time <= time_traits::now()) {
            res_it->value.reset();
            _available.splice(_wasted.end(), _wasted, res_it);
            --_available_size;
            continue;
        }
        _available.splice(_used.end(), _used, res_it);
        --_available_size;
        ++_used_size;
        lock.unlock();
        return res_it;
    }
    return list_iterator();
}

template <class T, class M, class C>
typename pool_impl<T, M, C>::list_iterator pool_impl<T, M, C>::reserve_resource(unique_lock& lock) {
    const list_iterator res_it = _wasted.begin();
    _wasted.splice(_used.end(), _used, res_it);
    ++_used_size;
    lock.unlock();
    return res_it;
}

template <class T, class M, class C>
bool pool_impl<T, M, C>::wait_for(unique_lock& lock, time_traits::duration wait_duration) {
    return _has_capacity.wait_for(lock, wait_duration) == std::cv_status::no_timeout;
}

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

}
}
}
}

#endif
