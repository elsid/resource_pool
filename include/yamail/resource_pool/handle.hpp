#ifndef YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP
#define YAMAIL_RESOURCE_POOL_SYNC_HANDLE_HPP

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace sync {

template <class ResourcePool>
class handle : public boost::enable_shared_from_this<handle<ResourcePool> >,
    boost::noncopyable {
public:
    typedef ResourcePool pool;
    typedef typename pool::pool_impl pool_impl;
    typedef typename pool::pool_impl_ptr pool_impl_ptr;
    typedef typename pool::resource resource;
    typedef typename pool::time_duration time_duration;
    typedef typename pool_impl::resource_opt resource_opt;
    typedef typename pool_impl::get_result get_result;
    typedef void (handle::*strategy)();

    handle(pool_impl_ptr pool_impl, strategy use_strategy,
            const time_duration& wait_duration)
            : _pool_impl(pool_impl), _use_strategy(use_strategy),
              _error(error::none)
    {
        const get_result result = _pool_impl->get(wait_duration);
        if (result.first == error::none) {
            _resource = result.second;
        } else {
            _error = result.first;
        }
    }

    ~handle();

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

private:
    pool_impl_ptr _pool_impl;
    strategy _use_strategy;
    resource_opt _resource;
    error::code _error;

    void assert_not_empty() const;
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
void handle<P>::assert_not_empty() const {
    if (empty()) {
        throw error::empty_handle();
    }
}

}}}

#endif
