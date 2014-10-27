#ifndef YAMAIL_RESOURCE_POOL_ASYNC_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_HANDLE_HPP

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class ResourcePool>
class handle : public boost::enable_shared_from_this<handle<ResourcePool> >,
    boost::noncopyable {
public:
    typedef ResourcePool pool;
    typedef boost::shared_ptr<handle> shared_ptr;
    typedef typename pool::pool_impl pool_impl;
    typedef typename pool::pool_impl_ptr pool_impl_ptr;
    typedef typename pool::resource resource;
    typedef typename pool::time_duration time_duration;
    typedef typename pool_impl::resource_opt resource_opt;
    typedef void (handle::*strategy)();
    typedef boost::function<void (shared_ptr)> callback;

    handle(pool_impl_ptr pool_impl, strategy use_strategy)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _error(error::none) {}

    ~handle();

    shared_ptr shared_from_this() {
        return boost::enable_shared_from_this<handle>::shared_from_this();
    }

    error::code error() const { return _error; }
    bool empty() const { return !_resource.is_initialized(); }
    resource& get();
    const resource& get() const;
    resource *operator ->() { return &get(); }
    const resource *operator ->() const { return &get(); }
    resource &operator *() { return get(); }
    const resource &operator *() const { return get(); }

    void recycle();
    void waste();
    void request(callback call, const time_duration& wait_duration);

private:
    pool_impl_ptr _pool_impl;
    callback _call;
    strategy _use_strategy;
    resource_opt _resource;
    error::code _error;

    void assert_not_empty() const;
    void set(callback call, const error::code& err, const resource_opt& res);
};

template <class P>
handle<P>::~handle() {
    if (!empty()) {
        (this->*_use_strategy)();
    }
}

template <class P>
typename handle<P>::resource& handle<P>::get() {
    assert_not_empty();
    return *_resource;
}

template <class P>
const typename handle<P>::resource& handle<P>::get() const {
    assert_not_empty();
    return *_resource;
}

template <class P>
void handle<P>::recycle() {
    assert_not_empty();
    _pool_impl->recycle(*_resource);
    _resource.reset();
}

template <class P>
void handle<P>::waste() {
    assert_not_empty();
    _pool_impl->waste(*_resource);
    _resource.reset();
}

template <class P>
void handle<P>::request(callback call, const time_duration& wait_duration) {
    _pool_impl->get(bind(&handle::set, shared_from_this(), call, _1, _2),
        wait_duration);
}

template <class P>
void handle<P>::assert_not_empty() const {
    if (empty()) {
        throw error::empty_handle();
    }
}

template <class P>
void handle<P>::set(callback call, const error::code& error,
        const resource_opt& resource) {
    _error = error;
    _resource = resource;
    _pool_impl->async_call(bind(call, shared_from_this()));
}

}}}

#endif
