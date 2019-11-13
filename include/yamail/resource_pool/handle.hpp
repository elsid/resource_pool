#ifndef YAMAIL_RESOURCE_POOL_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_HPP

#include <yamail/resource_pool/detail/pool_returns.hpp>
#include <yamail/resource_pool/detail/storage.hpp>
#include <yamail/resource_pool/error.hpp>

#include <memory>
#include <optional>

namespace yamail {
namespace resource_pool {

template <class T>
class handle {
public:
    using value_type = T;
    using strategy = void (handle::*)();
    using list_iterator = detail::cell_iterator<value_type>;

    handle() = default;
    handle(const handle& other) = delete;
    handle(handle&& other);

    handle(std::shared_ptr<detail::pool_returns<value_type>> pool_impl,
           strategy use_strategy,
           list_iterator resource_it)
            : _pool_impl(std::move(pool_impl)),
              _use_strategy(use_strategy),
              _resource_it(resource_it) {}

    ~handle();

    handle& operator =(const handle& other) = delete;
    handle& operator =(handle&& other);

    bool unusable() const noexcept { return !static_cast<bool>(_resource_it); }
    bool empty() const noexcept { return unusable() || !_resource_it.value()->value; }
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
    std::shared_ptr<detail::pool_returns<value_type>> _pool_impl;
    strategy _use_strategy;
    std::optional<list_iterator> _resource_it;

    void assert_not_empty() const;
    void assert_not_unusable() const;
};

template <class P>
handle<P>::handle(handle&& other)
    : _pool_impl(other._pool_impl),
      _use_strategy(other._use_strategy),
      _resource_it(other._resource_it) {
    other._resource_it = std::nullopt;
    other._pool_impl.reset();
}

template <class P>
handle<P>::~handle() {
    if (!unusable()) {
        (this->*_use_strategy)();
    }
}

template <class P>
handle<P>& handle<P>::operator =(handle&& other) {
    if (!unusable()) {
        (this->*_use_strategy)();
    }
    _pool_impl = other._pool_impl;
    _use_strategy = other._use_strategy;
    _resource_it = other._resource_it;
    other._resource_it = std::nullopt;
    other._pool_impl.reset();
    return *this;
}

template <class P>
typename handle<P>::value_type& handle<P>::get() {
    assert_not_empty();
    return *_resource_it.value()->value;
}

template <class P>
const typename handle<P>::value_type& handle<P>::get() const {
    assert_not_empty();
    return *_resource_it.value()->value;
}

template <class P>
void handle<P>::recycle() {
    assert_not_unusable();
    _pool_impl->recycle(_resource_it.value());
    _resource_it = std::nullopt;
}

template <class P>
void handle<P>::waste() {
    assert_not_unusable();
    _pool_impl->waste(_resource_it.value());
    _resource_it = std::nullopt;
}

template <class P>
void handle<P>::reset(value_type &&res) {
    assert_not_unusable();
    _resource_it.value()->value = std::move(res);
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
