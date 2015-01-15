#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <list>

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/resource_factory.hpp>
#include <yamail/resource_pool/async/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <
    class Resource,
    class ResourceAlloc = std::allocator<Resource> >
class pool {
public:
    typedef Resource resource;
    typedef ResourceAlloc resource_alloc;
    typedef detail::pool_impl<resource, resource_alloc> pool_impl;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef typename pool_impl::io_service io_service;
    typedef typename pool_impl::shared_ptr pool_impl_ptr;
    typedef async::resource_factory<resource> resource_factory;
    typedef async::handle<pool> handle;
    typedef typename handle::callback callback;
    typedef async::make_handle<resource> make_handle;
    typedef typename make_handle::callback_succeed make_resource_callback_succeed;
    typedef typename make_handle::callback_failed make_resource_callback_failed;
    typedef boost::shared_ptr<make_handle> make_handle_ptr;
    typedef boost::function<void (make_handle_ptr)> make_resource;

    pool(io_service& io_service, std::size_t capacity = 0,
            std::size_t queue_capacity = 0,
            make_resource make_res = resource_factory())
            : _impl(
                boost::make_shared<pool_impl>(boost::ref(io_service),
                capacity,
                queue_capacity,
                boost::bind(use_make_resource, make_res, _1, _2))) {}

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }
    std::size_t creating() const { return _impl->creating(); }

    std::size_t queue_capacity() const { return _impl->queue_capacity(); }
    std::size_t queue_size() const { return _impl->queue_size(); }
    bool queue_empty() const { return _impl->queue_empty(); }

    void get_auto_waste(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::waste, wait_duration);
    }

    void get_auto_recycle(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::recycle, wait_duration);
    }

private:
    typedef typename handle::strategy strategy;

    pool_impl_ptr _impl;

    void get(callback call, strategy use_strategy,
            const time_duration& wait_duration) {
        boost::make_shared<handle>(_impl, use_strategy)->request(call, wait_duration);
    }

    static void use_make_resource(make_resource make_res,
            make_resource_callback_succeed succeed,
            make_resource_callback_failed failed) {
        make_res(boost::make_shared<make_handle>(succeed, failed));
    }
};

}}}

#endif
