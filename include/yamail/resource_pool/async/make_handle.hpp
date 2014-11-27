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
            : _succeed(succeed), _failed(failed), _created_one(false) {}

    ~make_handle();

    void set(resource res);

private:
    callback_succeed _succeed;
    callback_failed _failed;
    bool _created_one;
};

template <class T>
make_handle<T>::~make_handle() {
    if (!_created_one) {
        _failed();
    }
}

template <class T>
void make_handle<T>::set(resource res) {
    _succeed(res);
    _created_one = true;
}

}}}

#endif
