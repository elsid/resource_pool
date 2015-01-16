#ifndef YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/sync/handle.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class Resource>
class pool {
public:
    typedef Resource resource;
    typedef detail::pool_impl<resource> pool_impl;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef sync::handle<resource> handle;
    typedef boost::shared_ptr<handle> handle_ptr;

    pool(std::size_t capacity = 0)
            : _impl(boost::make_shared<pool_impl>(capacity))
    {}

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }
    std::size_t reserved() const { return _impl->reserved(); }

    handle_ptr get_auto_waste(const time_duration& wait_duration = seconds(0)) {
        return get_handle(&handle::waste, wait_duration);
    }

    handle_ptr get_auto_recycle(const time_duration& wait_duration = seconds(0)) {
        return get_handle(&handle::recycle, wait_duration);
    }

private:
    typedef boost::shared_ptr<pool_impl> pool_impl_ptr;
    typedef void (handle::*strategy)();

    pool_impl_ptr _impl;

    handle_ptr get_handle(strategy use_strategy, const time_duration& wait_duration) {
        const typename pool_impl::get_result& res = _impl->get(wait_duration);
        return handle_ptr(new handle(_impl, use_strategy, res.second, res.first));
    }
};

}}}

#endif
