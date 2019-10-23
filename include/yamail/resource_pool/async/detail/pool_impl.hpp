#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/detail/storage.hpp>
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

template <class ListIterator, class Handler>
class on_list_iterator_handler {
    boost::system::error_code error;
    ListIterator list_iterator;
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    on_list_iterator_handler() = default;

    on_list_iterator_handler(const boost::system::error_code& error, ListIterator list_iterator, Handler handler)
        : error(std::move(error)),
          list_iterator(list_iterator),
          handler(std::move(handler)) {}

    template <class ... Args>
    void operator ()() {
        return handler(std::move(error), std::move(list_iterator));
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class ListIterator, class Handler>
auto make_on_list_iterator_handler(const boost::system::error_code& error, ListIterator list_iterator, Handler&& handler) {
    using result_type = on_list_iterator_handler<std::decay_t<ListIterator>, std::decay_t<Handler>>;
    return result_type(error, list_iterator, std::forward<Handler>(handler));
}

template <class ListIterator>
class queued_handler {
public:
    using executor_type = asio::executor;

    queued_handler() = default;

    template <class Handler>
    explicit queued_handler(Handler&& handler)
        : executor(asio::get_associated_executor(handler)),
          handler(std::forward<Handler>(handler)) {}

    void operator ()(const boost::system::error_code& error, ListIterator list_iterator) {
        handler(error, list_iterator);
    }

    auto get_executor() const noexcept {
        return executor;
    }

private:
    asio::executor executor;
    std::function<void (const boost::system::error_code&, ListIterator)> handler;
};

template <class ListIterator, class Handler>
auto make_queued_handler(Handler&& handler) {
    using result_type = queued_handler<ListIterator>;
    return result_type(std::forward<Handler>(handler));
}

template <class Value, class Mutex, class IoContext, class Queue>
class pool_impl {
public:
    using value_type = Value;
    using io_context_t = IoContext;
    using idle = resource_pool::detail::idle<value_type>;
    using storage_type = resource_pool::detail::storage<value_type>;
    using list_iterator = typename storage_type::cell_iterator;
    using queue_type = Queue;
    using queued_handler = typename queue_type::value_type;

    pool_impl(std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : storage_(assert_capacity(capacity), idle_timeout),
              _capacity(capacity),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {
    }

    template <class Generator>
    pool_impl(Generator&& gen_value,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : storage_(std::forward<Generator>(gen_value), assert_capacity(capacity), idle_timeout),
              _capacity(assert_capacity(capacity)),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {
    }

    template <class Iter>
    pool_impl(Iter first, Iter last,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : pool_impl([&]{ return std::move(*first++); },
                    static_cast<std::size_t>(std::distance(first, last)),
                    queue_capacity,
                    idle_timeout) {
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
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

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

    void perform_one_request(unique_lock& lock);
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
    storage_.recycle(res_it);
    perform_one_request(lock);
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::waste(list_iterator res_it) {
    unique_lock lock(_mutex);
    storage_.waste(res_it);
    perform_one_request(lock);
}

template <class V, class M, class I, class Q>
template <class Handler>
void pool_impl<V, M, I, Q>::get(io_context_t& io_context, Handler&& handler, time_traits::duration wait_duration) {
    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        asio::dispatch(io_context,
            make_on_list_iterator_handler(
                make_error_code(error::disabled),
                list_iterator(),
                std::forward<Handler>(handler)
            ));
        return;
    }
    if (const auto cell = storage_.lease()) {
        lock.unlock();
        asio::post(io_context,
            make_on_list_iterator_handler(
                boost::system::error_code(),
                *cell,
                std::forward<Handler>(handler)
            ));
        return;
    }
    lock.unlock();
    if (wait_duration.count() == 0) {
        asio::post(io_context,
            make_on_list_iterator_handler(
                make_error_code(error::get_resource_timeout),
                list_iterator(),
                std::forward<Handler>(handler)
            ));
        return;
    }
    const bool pushed = _callbacks->push(
        io_context,
        make_queued_handler<list_iterator>(handler),
        make_on_list_iterator_handler(
            make_error_code(error::get_resource_timeout),
            list_iterator(),
            handler
        ),
        wait_duration
    );
    if (pushed)
        return;
    asio::post(io_context,
        make_on_list_iterator_handler(
            make_error_code(error::request_queue_overflow),
            list_iterator(),
            std::forward<Handler>(handler)
        ));
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::disable() {
    const lock_guard lock(_mutex);
    _disabled = true;
    while (true) {
        io_context_t* io_context;
        queued_handler handler;
        if (!_callbacks->pop(io_context, handler)) {
            break;
        }
        asio::dispatch(*io_context,
            make_on_list_iterator_handler(
                make_error_code(error::disabled),
                list_iterator(),
                std::move(handler)
            ));
    }
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::assert_capacity(std::size_t value) {
    if (value == 0) {
        throw error::zero_pool_capacity();
    }
    return value;
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::perform_one_request(unique_lock& lock) {
    io_context_t* io_context;
    queued_handler handler;
    if (!_callbacks->pop(io_context, handler))
        return;
    const auto cell = storage_.lease();
    assert(cell);
    lock.unlock();
    asio::post(*io_context,
        make_on_list_iterator_handler(
            boost::system::error_code(),
            *cell,
            std::move(handler)
        ));
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
