#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <list>

#include <boost/make_shared.hpp>

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class T>
class pool {
public:
    typedef T value_type;
    typedef detail::pool_impl<value_type> pool_impl;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef resource_pool::handle<pool_impl> handle;
    typedef boost::shared_ptr<handle> handle_ptr;
    typedef boost::function<void (const boost::system::error_code&,
        handle_ptr)> callback;

    pool(boost::asio::io_service& io_service, std::size_t capacity = 0,
            std::size_t queue_capacity = 0)
            : _impl(boost::make_shared<pool_impl>(
                boost::ref(io_service),
                capacity,
                queue_capacity)) {}

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    std::size_t queue_capacity() const { return _impl->queue_capacity(); }
    std::size_t queue_size() const { return _impl->queue_size(); }
    bool queue_empty() const { return _impl->queue_empty(); }

    void get_auto_waste(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::waste, wait_duration);
    }

    void get_auto_recycle(callback call,
            const time_duration& wait_duration = seconds(0)) {
        return get(call, &handle::recycle, wait_duration);
    }

private:
    typedef typename pool_impl::list_iterator_opt list_iterator_opt;
    typedef typename handle::strategy strategy;
    typedef typename boost::shared_ptr<pool_impl> pool_impl_ptr;

    pool_impl_ptr _impl;

    void get(callback call, strategy use_strategy,
            const time_duration& wait_duration) {
        _impl->get(bind(make_handle, _impl, call, use_strategy, _1, _2),
            wait_duration);
    }

    static void make_handle(pool_impl_ptr impl, callback call,
            strategy use_strategy, const boost::system::error_code& ec,
            const list_iterator_opt& res) {
        try {
            impl->async_call(bind(call, ec,
                handle_ptr(new handle(impl, use_strategy, res))));
        } catch (...) {
            impl->async_call(bind(call,
                make_error_code(error::exception), handle_ptr()));
        }
    }
};

}
}
}

#endif
