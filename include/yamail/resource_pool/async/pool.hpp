#ifndef YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/handle.hpp>
#include <yamail/resource_pool/async/detail/pool_impl.hpp>

#include <boost/asio/io_context.hpp>

namespace yamail {
namespace resource_pool {
namespace async {

template <class Value, class Mutex, class IoContext>
struct default_pool_queue {
    using value_type = Value;
    using io_context_t = IoContext;
    using mutex_t = Mutex;
    using idle = resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = typename list::iterator;
    using type = detail::queue<detail::list_iterator_handler<value_type>, mutex_t, io_context_t, time_traits::timer>;
};

template <class Value, class Mutex, class IoContext>
struct default_pool_impl {
    using type = typename detail::pool_impl<
        Value,
        Mutex,
        IoContext,
        typename default_pool_queue<Value, Mutex, IoContext>::type
    >;
};

template <class Value,
          class Mutex = std::mutex,
          class IoContext = boost::asio::io_context,
          class Impl = typename default_pool_impl<Value, Mutex, IoContext>::type >
class pool {
public:
    using value_type = Value;
    using io_context_t = IoContext;
    using pool_impl = Impl;
    using handle = resource_pool::handle<value_type>;

    pool(std::size_t capacity,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max(),
         time_traits::duration lifespan = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                capacity,
                queue_capacity,
                idle_timeout,
                lifespan)) {}

    template <class Generator>
    pool(Generator&& gen_value,
         std::size_t capacity,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max(),
         time_traits::duration lifespan = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                std::forward<Generator>(gen_value),
                capacity,
                queue_capacity,
                idle_timeout,
                lifespan)) {}

    template <class Iter>
    pool(Iter first, Iter last,
         std::size_t queue_capacity,
         time_traits::duration idle_timeout = time_traits::duration::max(),
         time_traits::duration lifespan = time_traits::duration::max())
            : _impl(std::make_shared<pool_impl>(
                first, last,
                queue_capacity,
                idle_timeout,
                lifespan)) {}

    pool(std::shared_ptr<pool_impl> impl)
            : _impl(std::move(impl)) {}

    pool(const pool&) = delete;
    pool(pool&&) = default;

    ~pool() {
        if (_impl) {
            _impl->disable();
        }
    }

    pool& operator =(const pool&) = delete;
    pool& operator =(pool&&) = default;

    std::size_t capacity() const noexcept { return _impl->capacity(); }
    std::size_t size() const noexcept { return _impl->size(); }
    std::size_t available() const noexcept { return _impl->available(); }
    std::size_t used() const noexcept { return _impl->used(); }
    async::stats stats() const noexcept { return _impl->stats(); }

    const pool_impl& impl() const noexcept { return *_impl; }

    template <class CompletionToken>
    auto get_auto_waste(io_context_t& io_context, CompletionToken&& token,
                        time_traits::duration wait_duration = time_traits::duration(0)) {
        async_completion<CompletionToken> init(token);
        get(io_context, std::move(init.completion_handler), &handle::waste, wait_duration);
        return init.result.get();
    }

    template <class CompletionToken>
    auto get_auto_recycle(io_context_t& io_context, CompletionToken&& token,
                          time_traits::duration wait_duration = time_traits::duration(0)) {
        async_completion<CompletionToken> init(token);
        get(io_context, std::move(init.completion_handler), &handle::recycle, wait_duration);
        return init.result.get();
    }

private:
    using list_iterator = typename pool_impl::list_iterator;

    template <typename CompletionToken>
    using async_completion = detail::async_completion<CompletionToken, void (boost::system::error_code, handle)>;

    template <class UseStrategy, class Handler>
    class on_get_handler {
        std::shared_ptr<pool_impl> impl;
        UseStrategy use_strategy;
        Handler handler;

    public:
        using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

        template <class HandlerT>
        on_get_handler(std::shared_ptr<pool_impl> impl, UseStrategy use_strategy, HandlerT&& handler)
            : impl(std::move(impl)),
              use_strategy(std::move(use_strategy)),
              handler(std::forward<HandlerT>(handler)) {
            static_assert(std::is_same<std::decay_t<HandlerT>, Handler>::value, "HandlerT is not Handler");
        }

        void operator ()(boost::system::error_code ec, list_iterator res) {
            if (ec) {
                handler(ec, handle());
            } else {
                handler(ec, handle(impl, use_strategy, std::move(res)));
            }
        }

        auto get_executor() const noexcept {
            return asio::get_associated_executor(handler);
        }
    };

    template <class UseStrategy, class Handler>
    auto make_on_get_handler(UseStrategy&& use_strategy, Handler&& handler) {
        using result_type = on_get_handler<std::decay_t<UseStrategy>, std::decay_t<Handler>>;
        return result_type(_impl, std::forward<UseStrategy>(use_strategy), std::forward<Handler>(handler));
    }

    std::shared_ptr<pool_impl> _impl;

    template <class UseStrategy, class Handler>
    void get(io_context_t &io_context, Handler&& handler, UseStrategy&& use_strategy, time_traits::duration wait_duration) {
        _impl->get(
            io_context,
            make_on_get_handler(std::forward<UseStrategy>(use_strategy), std::forward<Handler>(handler)),
            wait_duration
        );
    }
};

} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_POOL_HPP
