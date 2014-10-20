#ifndef YAMAIL_RESOURCE_POOL_POOL_HPP
#define YAMAIL_RESOURCE_POOL_POOL_HPP

#include <set>

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {

template <class T>
struct object_factory {
    T operator()() { return T(); }
};

template <class Resource>
class pool {
public:
    typedef Resource resource;
    typedef detail::pool_impl<resource> pool_impl;
    typedef typename pool_impl::shared_ptr pool_impl_ptr;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef typename pool_impl::make_resource make_resource;
    typedef typename pool_impl::resource_opt resource_opt;
    typedef object_factory<resource> resource_factory;
    typedef resource_pool::handle<pool> handle;
    typedef boost::shared_ptr<handle> handle_ptr;

    pool(std::size_t capacity = 0, make_resource make_res = resource_factory())
            : _impl(boost::make_shared<pool_impl>(capacity, make_res))
    {}

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    void fill() { return _impl->fill(); }
    void clear() { return _impl->clear(); }

    handle_ptr get_auto_waste(const time_duration& wait_duration = seconds(0));
    handle_ptr get_auto_recycle(const time_duration& wait_duration = seconds(0));

private:
    typedef void (handle::*strategy)();

    pool_impl_ptr _impl;

    handle_ptr get_handle(strategy use_use_strategy, const time_duration& wait_duration);
};

template <class R>
typename pool<R>::handle_ptr pool<R>::get_auto_waste(
        const time_duration& wait_duration) {
    return get_handle(&handle::waste, wait_duration);
}

template <class R>
typename pool<R>::handle_ptr pool<R>::get_auto_recycle(
        const time_duration& wait_duration) {
    return get_handle(&handle::recycle, wait_duration);
}

template <class R>
typename pool<R>::handle_ptr pool<R>::get_handle(strategy use_use_strategy,
        const time_duration& wait_duration) {
    return handle_ptr(new handle(_impl, use_use_strategy, wait_duration));
}

}}

#endif
