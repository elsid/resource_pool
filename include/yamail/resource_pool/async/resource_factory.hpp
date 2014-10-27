#ifndef YAMAIL_RESOURCE_POOL_ASYNC_RESOURCE_FACTORY_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_RESOURCE_FACTORY_HPP

#include <boost/function.hpp>

#include <yamail/resource_pool/async/make_handle.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class T>
struct resource_factory {
    typedef T resource;
    typedef async::make_handle<resource> make_handle;
    typedef boost::shared_ptr<make_handle> make_handle_ptr;

    void operator()(make_handle_ptr handle) { handle->reset(T()); }
};

}}}

#endif
