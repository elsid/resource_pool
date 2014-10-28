#ifndef YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_POOL_HPP

#include <set>

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/object_factory.hpp>
#include <yamail/resource_pool/sync/handle.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {

template <
    class Resource,
    class ResourceCompare = std::less<Resource>,
    class ResourceAlloc = std::allocator<Resource> >
class pool {
public:
    typedef Resource resource;
    typedef ResourceCompare resource_compare;
    typedef ResourceAlloc resource_alloc;
    typedef detail::pool_impl<resource, resource_compare, resource_alloc> pool_impl;
    typedef typename pool_impl::shared_ptr pool_impl_ptr;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef typename pool_impl::make_resource make_resource;
    typedef object_factory<resource> resource_factory;
    typedef sync::handle<pool> handle;
    typedef boost::shared_ptr<handle> handle_ptr;

    pool(std::size_t capacity = 0, make_resource make_res = resource_factory())
            : _impl(boost::make_shared<pool_impl>(capacity, make_res))
    {}

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    handle_ptr get_auto_waste(const time_duration& wait_duration = seconds(0));
    handle_ptr get_auto_recycle(const time_duration& wait_duration = seconds(0));

private:
    typedef void (handle::*strategy)();

    pool_impl_ptr _impl;

    handle_ptr get_handle(strategy use_strategy, const time_duration& wait_duration);
};

template <class R, class C, class A>
typename pool<R, C, A>::handle_ptr pool<R, C, A>::get_auto_waste(
        const time_duration& wait_duration) {
    return get_handle(&handle::waste, wait_duration);
}

template <class R, class C, class A>
typename pool<R, C, A>::handle_ptr pool<R, C, A>::get_auto_recycle(
        const time_duration& wait_duration) {
    return get_handle(&handle::recycle, wait_duration);
}

template <class R, class C, class A>
typename pool<R, C, A>::handle_ptr pool<R, C, A>::get_handle(
        strategy use_strategy, const time_duration& wait_duration) {
    return handle_ptr(new handle(_impl, use_strategy, wait_duration));
}

}}}

#endif
