#ifndef YAMAIL_RESOURCE_POOL_DETAIL_IDLE_HPP
#define YAMAIL_RESOURCE_POOL_DETAIL_IDLE_HPP

#include <yamail/resource_pool/time_traits.hpp>

#include <boost/optional.hpp>

namespace yamail {
namespace resource_pool {
namespace detail {

template <class Value>
struct idle {
    using value_type = Value;

    boost::optional<value_type> value;
    time_traits::time_point drop_time;

    idle(time_traits::time_point drop_time = time_traits::time_point::max())
        : drop_time(drop_time) {}
    idle(value_type&& value, time_traits::time_point drop_time)
        : value(std::move(value)), drop_time(drop_time) {}
};

} // namespace detail
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_DETAIL_IDLE_HPP
