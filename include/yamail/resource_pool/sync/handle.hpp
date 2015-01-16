#ifndef YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP

#include <boost/noncopyable.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class Resource>
class pool;

template <class Resource>
class handle : boost::noncopyable {
public:
    typedef Resource resource;
    typedef typename detail::pool_impl<resource> pool_impl;
    typedef typename pool_impl::resource_ptr resource_ptr;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::resource_ptr_list_iterator_opt resource_ptr_list_iterator_opt;
    typedef typename pool_impl::get_result get_result;
    typedef boost::shared_ptr<pool_impl> pool_impl_ptr;
    typedef void (handle::*strategy)();

    friend class pool<resource>;

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
    void reset(resource_ptr res);

private:
    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    resource_ptr_list_iterator_opt _resource_it;
    error::code _error;

    handle(pool_impl_ptr pool_impl,
            strategy use_strategy,
            const resource_list_iterator_opt& _resource_it,
            const error::code& _error)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _resource_it(_resource_it), _error(_error) {}

    void assert_not_empty() const;
};

template <class R>
handle<R>::~handle() {
    if (!empty()) {
        (this->*_use_strategy)();
    }
}

template <class R>
typename handle<R>::resource& handle<R>::get() {
    assert_not_empty();
    return ***_resource_it;
}

template <class R>
const typename handle<R>::resource& handle<R>::get() const {
    assert_not_empty();
    return ***_resource_it;
}

template <class R>
void handle<R>::recycle() {
    assert_not_empty();
    _pool_impl->recycle(*_resource_it);
    _resource_it.reset();
}

template <class R>
void handle<R>::waste() {
    assert_not_empty();
    _pool_impl->waste(*_resource_it);
    _resource_it.reset();
}

template <class R>
void handle<R>::reset(resource_ptr res) {
    if (empty()) {
        _resource_it.reset(_pool_impl->add(res));
    } else {
        _resource_it.reset(_pool_impl->replace(*_resource_it, res));
    }
}

template <class R>
void handle<R>::assert_not_empty() const {
    if (empty()) {
        throw error::empty_handle();
    }
}

}}}

#endif
