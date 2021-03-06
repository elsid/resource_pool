#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/detail/storage.hpp>
#include <yamail/resource_pool/detail/pool_returns.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>

#include <cassert>
#include <type_traits>

namespace yamail {
namespace resource_pool {
namespace async {

struct stats {
    std::size_t size;
    std::size_t available;
    std::size_t used;
    std::size_t queue_size;
};

namespace detail {

namespace asio = boost::asio;

using resource_pool::detail::cell_iterator;
using resource_pool::detail::cell_value;
using resource_pool::detail::pool_returns;

template <class T, class Handler>
class on_list_iterator_handler {
    static_assert(std::is_invocable_v<Handler, boost::system::error_code, cell_iterator<T>>);

    boost::system::error_code error;
    cell_iterator<T> list_iterator;
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    on_list_iterator_handler() = default;

    template <class HandlerT>
    on_list_iterator_handler(boost::system::error_code error, cell_iterator<T> list_iterator, HandlerT&& handler)
        : error(error),
          list_iterator(list_iterator),
          handler(std::forward<HandlerT>(handler)) {}

    template <class ... Args>
    void operator ()() {
        return handler(error, list_iterator);
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class ListIterator, class Handler>
on_list_iterator_handler(boost::system::error_code, ListIterator, Handler&&)
    -> on_list_iterator_handler<cell_value<ListIterator>, std::decay_t<Handler>>;

template <class T>
struct base_list_iterator_handler_impl {
    virtual void operator ()(boost::system::error_code ec, cell_iterator<T> iterator) = 0;
    virtual ~base_list_iterator_handler_impl() = default;
};

template <class T, class Handler>
class list_iterator_handler_impl final : public base_list_iterator_handler_impl<T> {
    static_assert(std::is_invocable_v<Handler, boost::system::error_code, cell_iterator<T>>);

public:
    template <class HandlerT>
    list_iterator_handler_impl(HandlerT&& handler,
            std::enable_if_t<!std::is_same_v<std::decay_t<HandlerT>, list_iterator_handler_impl>, void*> = nullptr)
            : handler(std::forward<HandlerT>(handler)) {
        static_assert(std::is_same_v<std::decay_t<HandlerT>, Handler>, "HandlerT is not Handler");
    }

    void operator ()(boost::system::error_code ec, cell_iterator<T> iterator) final {
        handler(ec, iterator);
    }

private:
    Handler handler;
};

template <class T>
class list_iterator_handler {
public:
    using executor_type = asio::executor;

    list_iterator_handler() = default;

    template <class Handler>
    list_iterator_handler(Handler&& handler,
            std::enable_if_t<!std::is_same_v<std::decay_t<Handler>, list_iterator_handler>, void*> = nullptr)
            : executor(asio::get_associated_executor(handler)),
              impl(std::make_unique<list_iterator_handler_impl<T, std::decay_t<Handler>>>(std::forward<Handler>(handler))) {
    }

    void operator ()(boost::system::error_code ec, cell_iterator<T> iterator) {
        (*impl)(ec, iterator);
    }

    void operator ()(boost::system::error_code ec) {
        (*impl)(ec, cell_iterator<T>());
    }

    void operator ()(cell_iterator<T> iterator) {
        (*impl)(boost::system::error_code(), iterator);
    }

    auto get_executor() const noexcept {
        return executor;
    }

private:
    asio::executor executor;
    std::unique_ptr<base_list_iterator_handler_impl<T>> impl;
};

template <class Handler>
class on_error_handler {
    static_assert(std::is_invocable_v<Handler, boost::system::error_code>);

    boost::system::error_code error;
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    template <class HandlerT>
    on_error_handler(boost::system::error_code error, HandlerT&& handler)
            : error(error),
              handler(std::forward<HandlerT>(handler)) {
        static_assert(std::is_same_v<std::decay_t<HandlerT>, Handler>, "HandlerT is not Handler");
    }

    void operator ()() {
        return handler(error);
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class Handler>
on_error_handler(boost::system::error_code, Handler&&) -> on_error_handler<std::decay_t<Handler>>;

template <class T, class Handler>
class on_serve_queued_handler {
    static_assert(std::is_invocable_v<Handler, cell_iterator<T>>);

    cell_iterator<T> list_iterator;
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    template <class HandlerT>
    on_serve_queued_handler(cell_iterator<T> list_iterator, HandlerT&& handler)
            : list_iterator(list_iterator),
              handler(std::forward<HandlerT>(handler)) {
        static_assert(std::is_same_v<std::decay_t<HandlerT>, Handler>, "HandlerT is not Handler");
    }

    void operator ()() {
        return handler(list_iterator);
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class ListIterator, class Handler>
on_serve_queued_handler(ListIterator, Handler&&)
    -> on_serve_queued_handler<cell_value<ListIterator>, std::decay_t<Handler>>;

template <class Value, class Mutex, class IoContext, class Queue>
class pool_impl : public pool_returns<Value> {
public:
    using value_type = Value;
    using io_context_t = IoContext;
    using idle = resource_pool::detail::idle<value_type>;
    using storage_type = resource_pool::detail::storage<value_type>;
    using list_iterator = typename storage_type::cell_iterator;
    using queue_type = Queue;

    pool_impl(std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout,
              time_traits::duration lifespan)
            : storage_(assert_capacity(capacity), idle_timeout, lifespan),
              _capacity(capacity),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {
    }

    template <class Generator>
    pool_impl(Generator&& gen_value,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout,
              time_traits::duration lifespan)
            : storage_(std::forward<Generator>(gen_value), assert_capacity(capacity), idle_timeout, lifespan),
              _capacity(assert_capacity(capacity)),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {
    }

    template <class Iter>
    pool_impl(Iter first, Iter last,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout,
              time_traits::duration lifespan)
            : pool_impl([&]{ return std::move(*first++); },
                    static_cast<std::size_t>(std::distance(first, last)),
                    queue_capacity,
                    idle_timeout,
                    lifespan) {
    }

    pool_impl(const pool_impl&) = delete;

    pool_impl(pool_impl&&) = delete;

    std::size_t capacity() const noexcept { return _capacity; }
    std::size_t size() const noexcept;
    std::size_t available() const noexcept;
    std::size_t used() const noexcept;
    async::stats stats() const noexcept;

    const queue_type& queue() const noexcept { return *_callbacks; }

    template <class Handler>
    void get(io_context_t& io_context, Handler&& handler, time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it) final;
    void waste(list_iterator res_it) final;
    void disable();
    void invalidate();

    static std::size_t assert_capacity(std::size_t value);

private:
    using mutex_t = Mutex;
    using unique_lock = std::unique_lock<mutex_t>;
    using lock_guard = std::lock_guard<mutex_t>;

    mutable mutex_t _mutex;
    storage_type storage_;
    const std::size_t _capacity;
    std::shared_ptr<queue_type> _callbacks;
    bool _disabled = false;
};

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::size() const noexcept {
    const auto stats = [&] {
        const lock_guard lock(_mutex);
        return storage_.stats();
    } ();
    return stats.available + stats.used;
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::available() const noexcept {
    const lock_guard lock(_mutex);
    return storage_.stats().available;
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::used() const noexcept {
    const lock_guard lock(_mutex);
    return storage_.stats().used;
}

template <class V, class M, class I, class Q>
async::stats pool_impl<V, M, I, Q>::stats() const noexcept {
    const auto stats = [&] {
        const lock_guard lock(_mutex);
        return storage_.stats();
    } ();
    async::stats result;
    result.size = stats.available + stats.used;
    result.available = stats.available;
    result.used = stats.used;
    result.queue_size = _callbacks->size();
    return result;
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::recycle(list_iterator res_it) {
    unique_lock lock(_mutex);
    auto queued = _callbacks->pop();
    if (!queued) {
        storage_.recycle(res_it);
        return;
    }
    const auto valid = storage_.is_valid(res_it);
    lock.unlock();
    if (!valid) {
        res_it->value.reset();
    }
    asio::post(queued->io_context, on_serve_queued_handler(res_it, std::move(queued->request)));
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    auto queued = _callbacks->pop();
    if (!queued) {
        storage_.waste(res_it);
        return;
    }
    lock.unlock();
    res_it->value.reset();
    asio::post(queued->io_context, on_serve_queued_handler(res_it, std::move(queued->request)));
}

template <class V, class M, class I, class Q>
template <class Handler>
void pool_impl<V, M, I, Q>::get(io_context_t& io_context, Handler&& handler, time_traits::duration wait_duration) {
    static_assert(std::is_invocable_v<std::decay_t<Handler>, boost::system::error_code, list_iterator>);

    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        asio::dispatch(io_context,
            on_list_iterator_handler(
                make_error_code(error::disabled),
                list_iterator(),
                std::forward<Handler>(handler)
            ));
        return;
    }
    if (const auto cell = storage_.lease()) {
        lock.unlock();
        asio::post(io_context,
            on_list_iterator_handler(
                boost::system::error_code(),
                *cell,
                std::forward<Handler>(handler)
            ));
        return;
    }
    lock.unlock();
    if (wait_duration.count() == 0) {
        asio::post(io_context,
            on_list_iterator_handler(
                make_error_code(error::get_resource_timeout),
                list_iterator(),
                std::forward<Handler>(handler)
            ));
        return;
    }
    list_iterator_handler<value_type> wrapped(std::forward<Handler>(handler));
    const bool pushed = _callbacks->push(io_context, wait_duration, std::move(wrapped));
    if (pushed) {
        return;
    }
    asio::post(io_context,
        on_error_handler(
            make_error_code(error::request_queue_overflow),
            std::move(wrapped)
        ));
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    while (true) {
        auto queued = _callbacks->pop();
        if (!queued) {
            break;
        }
        asio::dispatch(queued->io_context,
            on_error_handler(
                make_error_code(error::disabled),
                std::move(queued->request)
            ));
    }
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::invalidate() {
    const lock_guard lock(_mutex);
    storage_.invalidate();
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

using boost::asio::async_result;
template<typename CompletionToken, typename Signature>
struct handler_type {
    using type = typename boost::asio::async_result<CompletionToken, Signature>::completion_handler_type;
};

#if BOOST_VERSION < 106600
template <typename CompletionToken, typename Signature>
struct async_completion {
    explicit async_completion(CompletionToken& token)
    : completion_handler(std::move(token)),
      result(completion_handler) {
    }

    using completion_handler_type = typename handler_type<CompletionToken, Signature>::type;

    completion_handler_type completion_handler;
    async_result<completion_handler_type> result;
};
#else
using boost::asio::async_completion;
#endif

template <typename Handler, typename Signature>
using async_return_type = typename ::boost::asio::async_result<Handler, Signature>::return_type;

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
