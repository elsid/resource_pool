#ifndef YAMAIL_RESOURCE_POOL_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_HPP

#include <yamail/resource_pool/error.hpp>

#include <memory>

namespace yamail {
namespace resource_pool {

template <class PoolImpl>
class handle {
public:
    using pool_impl = PoolImpl;
    using value_type = typename pool_impl::value_type;
    using strategy = void (handle::*)();
    using pool_impl_ptr = std::shared_ptr<pool_impl>;
    using list_iterator = typename pool_impl::list_iterator;

    handle() = default;
    handle(const handle& other) = delete;
    handle(handle&& other);

    handle(const pool_impl_ptr& pool_impl,
           strategy use_strategy,
           list_iterator resource_it)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _resource_it(resource_it) {}

    virtual ~handle();

    handle& operator =(const handle& other) = delete;
    handle& operator =(handle&& other);

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
    void reset(value_type&& res);

private:
    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    list_iterator _resource_it;

    void assert_not_empty() const;
    void assert_not_unusable() const;
};

template <class P>
handle<P>::handle(handle&& other)
    : _pool_impl(other._pool_impl),
      _use_strategy(other._use_strategy),
      _resource_it(other._resource_it) {
    other._resource_it = list_iterator();
}

template <class P>
handle<P>::~handle() {
    if (!unusable()) {
        (this->*_use_strategy)();
    }
}

template <class P>
handle<P>& handle<P>::operator =(handle&& other) {
    _pool_impl = other._pool_impl;
    _use_strategy = other._use_strategy;
    _resource_it = other._resource_it;
    other._resource_it = list_iterator();
    return *this;
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
void handle<P>::reset(value_type &&res) {
    assert_not_unusable();
    _resource_it->value = std::move(res);
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
