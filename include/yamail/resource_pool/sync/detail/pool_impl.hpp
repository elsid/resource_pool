#ifndef YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/detail/storage.hpp>
#include <yamail/resource_pool/detail/pool_returns.hpp>

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

using resource_pool::detail::pool_returns;

template <class Value, class Mutex, class ConditionVariable>
class pool_impl : public pool_returns<Value> {
public:
    using value_type = Value;
    using condition_variable = ConditionVariable;
    using idle = resource_pool::detail::idle<value_type>;
    using storage_type = resource_pool::detail::storage<value_type>;
    using list_iterator = typename storage_type::cell_iterator;
    using get_result = std::pair<boost::system::error_code, list_iterator>;

    pool_impl(std::size_t capacity, time_traits::duration idle_timeout)
            : storage_(assert_capacity(capacity), idle_timeout),
              _capacity(capacity),
              _disabled(false) {
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    sync::stats stats() const;

    const condition_variable& has_capacity() const { return _has_capacity; }

    get_result get(time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it) final;
    void waste(list_iterator res_it) final;
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    using mutex_t = Mutex;
    using lock_guard = std::lock_guard<mutex_t>;
    using unique_lock = std::unique_lock<mutex_t>;

    mutable mutex_t _mutex;
    storage_type storage_;
    const std::size_t _capacity;
    condition_variable _has_capacity;
    bool _disabled;

    bool wait_for(unique_lock& lock, time_traits::duration wait_duration);
};

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::size() const {
    const auto stats = [&] {
        const lock_guard lock(_mutex);
        return storage_.stats();
    } ();
    return stats.available + stats.used;
}

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::available() const {
    const lock_guard lock(_mutex);
    return storage_.stats().available;
}

template <class T, class M, class C>
std::size_t pool_impl<T, M, C>::used() const {
    const lock_guard lock(_mutex);
    return storage_.stats().used;
}

template <class T, class M, class C>
sync::stats pool_impl<T, M, C>::stats() const {
    const auto stats = [&] {
        const lock_guard lock(_mutex);
        return storage_.stats();
    } ();
    sync::stats result;
    result.size = stats.available + stats.used;
    result.available = stats.available;
    result.used = stats.used;
    return result;
}

template <class T, class M, class C>
void pool_impl<T, M, C>::recycle(list_iterator res_it) {
    const lock_guard lock(_mutex);
    storage_.recycle(res_it);
    _has_capacity.notify_one();
}

template <class T, class M, class C>
void pool_impl<T, M, C>::waste(list_iterator res_it) {
    const lock_guard lock(_mutex);
    storage_.waste(res_it);
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
        } 
        if (const auto cell = storage_.lease()) {
            lock.unlock();
            return std::make_pair(boost::system::error_code(), *cell);
        }
        if (!wait_for(lock, wait_duration)) {
            lock.unlock();
            return std::make_pair(make_error_code(error::get_resource_timeout),
                                  list_iterator());
        }
    }
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
