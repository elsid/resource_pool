#ifndef YAMAIL_RESOURCE_POOL_ASYNC_MAKE_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_MAKE_HANDLE_HPP

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class T>
class make_handle : boost::noncopyable {
public:
    typedef T resource;
    typedef boost::function<void (resource)> callback_succeed;
    typedef boost::function<void ()> callback_failed;
    typedef boost::optional<resource> resource_opt;

    make_handle(callback_succeed succeed, callback_failed failed)
            : _succeed(succeed), _failed(failed) {}

    ~make_handle() { _resource ? _succeed(*_resource) : _failed(); }

    void reset() { _resource.reset(); }
    void reset(resource res) { _resource.reset(res); }

private:
    callback_succeed _succeed;
    callback_failed _failed;
    resource_opt _resource;
};

}}}

#endif
