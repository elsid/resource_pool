#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class Value, class Mutex, class IoService>
struct default_pool_queue {
    using value_type = Value;
    using io_service_t = IoService;
    using mutex_t = Mutex;
    using idle = resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = typename list::iterator ;
    using callback = std::function<void (const boost::system::error_code&, list_iterator)>;
    using type = detail::queue<callback, mutex_t, io_service_t, time_traits::timer>;
};

template <class Value, class Mutex, class IoService>
struct default_pool_impl {
    using type = typename detail::pool_impl<
        Value,
        Mutex,
        IoService,
        typename default_pool_queue<Value, Mutex, IoService>::type
    >;
};

template <class Value,
          class Mutex = std::mutex,
          class IoService = boost::asio::io_service,
          class Impl = typename default_pool_impl<Value, Mutex, IoService>::type >
class pool {
public:
    using value_type = Value;
    using io_service_t = IoService;
    using pool_impl = Impl;
    using handle = resource_pool::handle<pool>;

    pool(std::size_t capacity,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                capacity,
                queue_capacity,
                idle_timeout)) {}

    template <class Generator>
    pool(Generator&& gen_value,
         std::size_t capacity,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                std::forward<Generator>(gen_value),
                capacity,
                queue_capacity,
                idle_timeout)) {}

    template <class Iter>
    pool(Iter first, Iter last,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                first, last,
                queue_capacity,
                idle_timeout)) {}

    pool(const pool&) = delete;
    pool(pool&&) = default;

    ~pool() {
        if (_impl) {
            _impl->disable();
        }
    }

    pool& operator =(const pool&) = delete;
    pool& operator =(pool&&) = default;

    std::size_t capacity() const { return _impl->capacity(); }
    std::size_t size() const { return _impl->size(); }
    std::size_t available() const { return _impl->available(); }
    std::size_t used() const { return _impl->used(); }
    async::stats stats() const { return _impl->stats(); }

    const pool_impl& impl() const { return *_impl; }

    using cb_signature = void(boost::system::error_code, handle);
    template <typename Callback>
    using return_type = detail::async_return_type<Callback, cb_signature>;

    template <class Callback>
    return_type<Callback> get_auto_waste(io_service_t& io_service, Callback call,
                        time_traits::duration wait_duration = time_traits::duration(0)) {
        async_result_init<Callback> init(std::move(call));
        get(io_service, init.handler, &handle::waste, wait_duration);
        return init.result.get();
    }

    template <class Callback>
    return_type<Callback> get_auto_recycle(io_service_t& io_service, Callback call,
                          time_traits::duration wait_duration = time_traits::duration(0)) {
        async_result_init<Callback> init(std::move(call));
        get(io_service, init.handler, &handle::recycle, wait_duration);
        return init.result.get();
    }

private:
    using list_iterator = typename pool_impl::list_iterator;
    using strategy = typename handle::strategy;

    template <typename Callback>
    using async_result_init = detail::async_result_init<Callback, cb_signature>;

    std::shared_ptr<pool_impl> _impl;

    template <class Callback>
    void get(io_service_t &io_service, Callback call, strategy use_strategy, time_traits::duration wait_duration) {
        using boost::asio::asio_handler_invoke;
        const auto impl = _impl;
        const auto on_get = [impl, use_strategy, call] (const boost::system::error_code& ec, list_iterator res) mutable {
            if (ec) {
                asio_handler_invoke([=] () mutable { call(ec, handle()); }, &call);
            } else {
                asio_handler_invoke([=] () mutable { call(ec, handle(impl, use_strategy, res)); }, &call);
            }
        };
        _impl->get(io_service, on_get, wait_duration);
    }
};

} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
