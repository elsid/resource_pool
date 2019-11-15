#ifndef YAMAIL_RESOURCE_POOL_DETAIL_POOL_RETURNS_HPP
#define YAMAIL_RESOURCE_POOL_DETAIL_POOL_RETURNS_HPP

#include <yamail/resource_pool/detail/storage.hpp>

namespace yamail {
namespace resource_pool {
namespace detail {

template <class T>
struct pool_returns {
    virtual ~pool_returns() = default;

    virtual void waste(cell_iterator<T> resource_iterator) = 0;

    virtual void recycle(cell_iterator<T> resource_iterator) = 0;
};

} // namespace detail
} // namespace resource_pool
} // namespace yamail

#endif
