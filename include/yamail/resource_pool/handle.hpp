#ifndef YAMAIL_RESOURCE_POOL_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_HPP

#include <yamail/resource_pool/error.hpp>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace yamail {
namespace resource_pool {

template <class Pool>
class handle : boost::noncopyable {
public:
    typedef Pool pool;
    typedef typename pool::pool_impl pool_impl;
    typedef typename pool_impl::value_type value_type;
    typedef typename pool_impl::pointer pointer;
    typedef void (handle::*strategy)();

    friend pool;

    virtual ~handle();

    bool unusable() const { return _resource_it == list_iterator(); }
    bool empty() const { return unusable() || !_resource_it->value; }
    value_type& get();
    const value_type& get() const;
    value_type *operator ->() { return &get(); }
    const value_type *operator ->() const { return &get(); }
    value_type &operator *() { return get(); }
    const value_type &operator *() const { return get(); }

    void recycle();
    void waste();
    void reset(const pointer& res);

protected:
    typedef boost::shared_ptr<pool_impl> pool_impl_ptr;
    typedef typename pool_impl::list_iterator list_iterator;

    handle(const pool_impl_ptr& pool_impl,
           strategy use_strategy,
           list_iterator resource_it)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _resource_it(resource_it) {}

private:
    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    list_iterator _resource_it;

    void assert_not_empty() const;
    void assert_not_unusable() const;
};

template <class P>
handle<P>::~handle() {
    if (!unusable()) {
        (this->*_use_strategy)();
    }
}

template <class P>
typename handle<P>::value_type& handle<P>::get() {
    assert_not_empty();
    return *_resource_it->value;
}

template <class P>
const typename handle<P>::value_type& handle<P>::get() const {
    assert_not_empty();
    return *_resource_it->value;
}

template <class P>
void handle<P>::recycle() {
    assert_not_unusable();
    _pool_impl->recycle(_resource_it);
    _resource_it = list_iterator();
}

template <class P>
void handle<P>::waste() {
    assert_not_unusable();
    _pool_impl->waste(_resource_it);
    _resource_it = list_iterator();
}

template <class P>
void handle<P>::reset(const pointer &res) {
    assert_not_unusable();
    _resource_it->value = res;
}

template <class P>
void handle<P>::assert_not_empty() const {
    if (empty()) {
        throw error::empty_handle();
    }
}

template <class P>
void handle<P>::assert_not_unusable() const {
    if (unusable()) {
        throw error::unusable_handle();
    }
}

}
}

#endif
