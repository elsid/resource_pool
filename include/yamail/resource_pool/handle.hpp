#ifndef YAMAIL_RESOURCE_POOL_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_HPP

#include <boost/noncopyable.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {

namespace sync {
template <class T>
class pool;
}

namespace async {
template <class T>
class pool;
}

template <class ResourcePoolImpl>
class handle : boost::noncopyable {
public:
    typedef ResourcePoolImpl pool_impl;
    typedef typename pool_impl::value_type value_type;
    typedef typename pool_impl::pointer pointer;
    typedef void (handle::*strategy)();

    friend class sync::pool<value_type>;
    friend class async::pool<value_type>;

    ~handle();

    error::code error() const { return _error; }
    bool empty() const { return !_resource_it.is_initialized(); }
    value_type& get();
    const value_type& get() const;
    value_type *operator ->() { return &get(); }
    const value_type *operator ->() const { return &get(); }
    value_type &operator *() { return get(); }
    const value_type &operator *() const { return get(); }

    void recycle();
    void waste();
    void reset(pointer res);

private:
    typedef boost::shared_ptr<pool_impl> pool_impl_ptr;
    typedef typename pool_impl::list_iterator_opt list_iterator_opt;

    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    list_iterator_opt _resource_it;
    error::code _error;

    handle(pool_impl_ptr pool_impl,
            strategy use_strategy,
            const list_iterator_opt& _resource_it,
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
typename handle<R>::value_type& handle<R>::get() {
    assert_not_empty();
    return ***_resource_it;
}

template <class R>
const typename handle<R>::value_type& handle<R>::get() const {
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
void handle<R>::reset(pointer res) {
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

}
}

#endif
