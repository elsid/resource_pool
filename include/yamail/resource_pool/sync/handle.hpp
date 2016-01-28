#ifndef YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP

#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/sync/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class T>
class pool;

template <class T>
class handle : public resource_pool::handle<sync::pool<T> > {
public:
    typedef T value_type;
    typedef resource_pool::handle<sync::pool<value_type> > base;

    friend class sync::pool<value_type>;

    const boost::system::error_code& error() const { return _error; }

private:
    boost::system::error_code _error;

    handle(typename base::pool_impl_ptr pool_impl,
            typename base::strategy use_strategy,
            const typename base::list_iterator_opt& resource_it,
            const boost::system::error_code& error)
            : base(pool_impl, use_strategy, resource_it), _error(error) {}
};

}
}
}

#endif
