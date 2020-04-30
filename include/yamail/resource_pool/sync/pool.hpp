#ifndef YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

#include <condition_variable>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class Value,
          class Mutex = std::mutex,
          class Impl = detail::pool_impl<Value, Mutex, std::condition_variable>>
class pool {
public:
    using value_type = Value;
    using pool_impl = Impl;
    using handle = resource_pool::handle<value_type>;
    using get_result = std::pair<boost::system::error_code, handle>;

    pool(std::size_t capacity,
         time_traits::duration idle_timeout = time_traits::duration::max(),
         time_traits::duration lifespan = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(capacity, idle_timeout, lifespan))
    {}

    pool(std::shared_ptr<pool_impl> impl)
            : _impl(std::move(impl))
    {}

    pool(const pool&) = delete;
    pool(pool&&) = default;

    ~pool() {
        if (_impl) {
            _impl->disable();
        }
    }

    pool& operator =(const pool&) = delete;
    pool& operator =(pool&&) = default;

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }
    sync::stats stats() const { return _impl->stats(); }

    get_result get_auto_waste(time_traits::duration wait_duration = time_traits::duration(0)) {
        return get_handle(&handle::waste, wait_duration);
    }

    get_result get_auto_recycle(time_traits::duration wait_duration = time_traits::duration(0)) {
        return get_handle(&handle::recycle, wait_duration);
    }

    void invalidate() {
        _impl->invalidate();
    }

private:
    using strategy = typename handle::strategy;
    using pool_impl_ptr = std::shared_ptr<pool_impl>;

    pool_impl_ptr _impl;

    get_result get_handle(strategy use_strategy, time_traits::duration wait_duration) {
        const typename pool_impl::get_result& res = _impl->get(wait_duration);
        return std::make_pair(res.first, handle(_impl, use_strategy, res.second));
    }
};

}
}
}

#endif
