#ifndef YAMAIL_RESOURCE_POOL_ASYNC_RESOURCE_FACTORY_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_RESOURCE_FACTORY_HPP

#include <boost/function.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/make_handle.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class T>
struct resource_factory {
    typedef T resource;
    typedef async::make_handle<resource> make_handle;
    typedef boost::shared_ptr<make_handle> make_handle_ptr;

    void operator()(make_handle_ptr handle);
};

template <class T>
void resource_factory<T>::operator()(make_handle_ptr handle) {
    try {
        handle->set(T());
    } catch (const error::add_existing_resource&) {}
}

}}}

#endif
