#ifndef YAMAIL_RESOURCE_POOL_ASYNC_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_HANDLE_HPP

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class Resource, class ResourceAlloc>
class pool;

template <class Resource, class ResourceAlloc>
class handle : boost::noncopyable {
public:
    typedef Resource resource;
    typedef ResourceAlloc resource_alloc;
    typedef typename detail::pool_impl<resource, resource_alloc> pool_impl;
    typedef typename pool_impl::shared_ptr pool_impl_ptr;
    typedef typename pool_impl::resource_list_iterator resource_list_iterator;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::resource_list_iterator_opt resource_list_iterator_opt;
    typedef void (handle::*strategy)();

    friend class async::pool<resource, resource_alloc>;

    ~handle();

    error::code error() const { return _error; }
    bool empty() const { return !_resource_it.is_initialized(); }
    resource& get();
    const resource& get() const;
    resource *operator ->() { return &get(); }
    const resource *operator ->() const { return &get(); }
    resource &operator *() { return get(); }
    const resource &operator *() const { return get(); }

    void recycle();
    void waste();
    void reset(resource res);

private:
    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    resource_list_iterator_opt _resource_it;
    error::code _error;

    handle(pool_impl_ptr pool_impl,
            strategy use_strategy,
            const resource_list_iterator_opt& _resource_it,
            const error::code& _error)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _resource_it(_resource_it), _error(_error) {}

    void assert_not_empty() const;
};

template <class R, class A>
handle<R, A>::~handle() {
    if (!empty()) {
        (this->*_use_strategy)();
    }
}

template <class R, class A>
typename handle<R, A>::resource& handle<R, A>::get() {
    assert_not_empty();
    return **_resource_it;
}

template <class R, class A>
const typename handle<R, A>::resource& handle<R, A>::get() const {
    assert_not_empty();
    return **_resource_it;
}

template <class R, class A>
void handle<R, A>::recycle() {
    assert_not_empty();
    _pool_impl->recycle(*_resource_it);
    _resource_it.reset();
}

template <class R, class A>
void handle<R, A>::waste() {
    assert_not_empty();
    _pool_impl->waste(*_resource_it);
    _resource_it.reset();
}

template <class R, class A>
void handle<R, A>::reset(resource res) {
    if (empty()) {
        _resource_it.reset(_pool_impl->add(res));
    } else {
        _resource_it.reset(_pool_impl->replace(*_resource_it, res));
    }
}

template <class R, class A>
void handle<R, A>::assert_not_empty() const {
    if (empty()) {
        throw error::empty_handle();
    }
}

}}}

#endif
