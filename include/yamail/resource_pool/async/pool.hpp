#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class Value>
struct default_pool_queue {
    typedef Value value_type;
    typedef boost::asio::io_service io_service_t;
    typedef boost::asio::steady_timer timer_t;
    typedef boost::shared_ptr<value_type> pointer;
    typedef std::list<pointer> list;
    typedef typename list::iterator list_iterator;
    typedef boost::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef detail::queue<callback, io_service_t, timer_t> type;
};

template <class Value,
          class IoService = boost::asio::io_service,
          class Impl = typename detail::pool_impl<Value, IoService, typename default_pool_queue<Value>::type> >
class pool : boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Impl pool_impl;
    typedef typename pool_impl::time_duration time_duration;
    typedef typename pool_impl::seconds seconds;
    typedef resource_pool::handle<pool> handle;
    typedef boost::shared_ptr<handle> handle_ptr;
    typedef boost::function<void (const boost::system::error_code&,
        const handle_ptr&)> callback;

    pool(io_service_t& io_service,
         std::size_t capacity,
         std::size_t queue_capacity)
            : _impl(boost::make_shared<pool_impl>(
                boost::ref(io_service),
                capacity,
                queue_capacity)) {}

    ~pool() { _impl->disable(); }

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }

    const pool_impl& impl() const { return *_impl; }

    void get_auto_waste(const callback& call,
                        time_duration wait_duration = seconds(0)) {
        return get(call, &handle::waste, wait_duration);
    }

    void get_auto_recycle(const callback& call,
                          time_duration wait_duration = seconds(0)) {
        return get(call, &handle::recycle, wait_duration);
    }

private:
    typedef typename pool_impl::list_iterator list_iterator;
    typedef typename handle::strategy strategy;
    typedef typename boost::shared_ptr<pool_impl> pool_impl_ptr;

    pool_impl_ptr _impl;

    void get(const callback& call, strategy use_strategy,
            time_duration wait_duration) {
        const handle_ptr h(new handle(_impl, use_strategy, list_iterator()));
        _impl->get(bind(fill_handle, call, h, _1, _2), wait_duration);
    }

    static void fill_handle(const callback& call, const handle_ptr& h,
            const boost::system::error_code& ec, list_iterator res) {
        if (ec) {
            return call(ec, handle_ptr());
        }
        h->_resource_it = res;
        call(ec, h);
    }
};

} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
