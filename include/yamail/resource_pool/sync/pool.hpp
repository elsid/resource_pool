#ifndef YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/sync/handle.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

#include <condition_variable>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class Value,
          class Impl = detail::pool_impl<Value, std::condition_variable> >
class pool {
public:
    typedef Value value_type;
    typedef Impl pool_impl;
    typedef sync::handle<value_type, pool_impl> handle;

    pool(std::size_t capacity, time_traits::duration idle_timeout = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(capacity, idle_timeout))
    {}

    ~pool() { _impl->disable(); }

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    const pool_impl& impl() const { return *_impl; }

    handle get_auto_waste(time_traits::duration wait_duration = time_traits::duration(0)) {
        return get_handle(&handle::waste, wait_duration);
    }

    handle get_auto_recycle(time_traits::duration wait_duration = time_traits::duration(0)) {
        return get_handle(&handle::recycle, wait_duration);
    }

private:
    typedef std::shared_ptr<pool_impl> pool_impl_ptr;
    typedef void (handle::base::*strategy)();

    pool_impl_ptr _impl;

    handle get_handle(strategy use_strategy, time_traits::duration wait_duration) {
        const typename pool_impl::get_result& res = _impl->get(wait_duration);
        return handle(_impl, use_strategy, res.second, res.first);
    }
};

}
}
}

#endif
