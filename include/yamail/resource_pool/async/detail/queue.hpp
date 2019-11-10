#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>

#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>

#include <algorithm>
#include <list>
#include <map>
#include <mutex>
#include <unordered_map>

namespace yamail {
namespace resource_pool {
namespace async {

namespace asio = boost::asio;

namespace detail {

using clock = std::chrono::steady_clock;

template <class Handler>
class expired_handler {
    Handler handler;

public:
    using executor_type = std::decay_t<decltype(asio::get_associated_executor(handler))>;

    expired_handler() = default;

    template <class HandlerT>
    explicit expired_handler(HandlerT&& handler,
            std::enable_if_t<!std::is_same_v<std::decay_t<HandlerT>, expired_handler>, void*> = nullptr)
            : handler(std::forward<HandlerT>(handler)) {
        static_assert(std::is_same_v<std::decay_t<HandlerT>, Handler>, "HandlerT is not Handler");
    }

    void operator ()() {
        handler(make_error_code(error::get_resource_timeout));
    }

    void operator ()() const {
        handler(make_error_code(error::get_resource_timeout));
    }

    auto get_executor() const noexcept {
        return asio::get_associated_executor(handler);
    }
};

template <class Handler>
expired_handler(Handler&&) -> expired_handler<std::decay_t<Handler>>;

template <class Value, class Mutex, class IoContext, class Timer>
class queue : public std::enable_shared_from_this<queue<Value, Mutex, IoContext, Timer>> {
public:
    using value_type = Value;
    using io_context_t = IoContext;
    using timer_t = Timer;

    queue(std::size_t capacity) : _capacity(capacity) {}

    queue(const queue&) = delete;

    queue(queue&&) = delete;

    std::size_t capacity() const noexcept { return _capacity; }
    std::size_t size() const noexcept;
    bool empty() const noexcept;
    const timer_t& timer(io_context_t& io_context);

    bool push(io_context_t& io_context, time_traits::duration wait_duration, value_type&& request);
    bool pop(io_context_t*& io_context, value_type& request);

private:
    using mutex_t = Mutex;
    using lock_guard = std::lock_guard<mutex_t>;

    struct expiring_request {
        using list = std::list<expiring_request>;
        using list_it = typename list::iterator;
        using multimap = std::multimap<time_traits::time_point, expiring_request*>;
        using multimap_it = typename multimap::iterator;

        io_context_t* io_context;
        queue::value_type request;
        list_it order_it;
        multimap_it expires_at_it;

        expiring_request() = default;
    };

    using request_multimap_value = typename expiring_request::multimap::value_type;
    using timers_map = typename std::unordered_map<const io_context_t*, timer_t>;

    const std::size_t _capacity;
    mutable mutex_t _mutex;
    typename expiring_request::list _ordered_requests_pool;
    typename expiring_request::list _ordered_requests;
    typename expiring_request::multimap _expires_at_requests;
    timers_map _timers;

    bool fit_capacity() const { return _expires_at_requests.size() < _capacity; }
    void cancel(boost::system::error_code ec, time_traits::time_point expires_at);
    void update_timer();
    timer_t& get_timer(io_context_t& io_context);
};

template <class V, class M, class I, class T>
std::size_t queue<V, M, I, T>::size() const noexcept {
    const lock_guard lock(_mutex);
    return _expires_at_requests.size();
}

template <class V, class M, class I, class T>
bool queue<V, M, I, T>::empty() const noexcept {
    const lock_guard lock(_mutex);
    return _ordered_requests.empty();
}

template <class V, class M, class I, class T>
const typename queue<V, M, I, T>::timer_t& queue<V, M, I, T>::timer(io_context_t& io_context) {
    const lock_guard lock(_mutex);
    return get_timer(io_context);
}

template <class V, class M, class I, class T>
bool queue<V, M, I, T>::push(io_context_t& io_context, time_traits::duration wait_duration, value_type&& request) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return false;
    }
    if (_ordered_requests_pool.empty()) {
        _ordered_requests_pool.emplace_back();
    }
    const auto order_it = _ordered_requests_pool.begin();
    _ordered_requests.splice(_ordered_requests.end(), _ordered_requests_pool, order_it);
    expiring_request& req = *order_it;
    req.io_context = std::addressof(io_context);
    req.request = std::move(request);
    req.order_it = order_it;
    const auto expires_at = time_traits::add(time_traits::now(), wait_duration);
    req.expires_at_it = _expires_at_requests.insert(std::make_pair(expires_at, &req));
    update_timer();
    return true;
}

template <class V, class M, class I, class T>
bool queue<V, M, I, T>::pop(io_context_t*& io_context, value_type& request) {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return false;
    }
    const auto ordered_it = _ordered_requests.begin();
    expiring_request& req = *ordered_it;
    io_context = req.io_context;
    request = std::move(req.request);
    _expires_at_requests.erase(req.expires_at_it);
    _ordered_requests_pool.splice(_ordered_requests_pool.begin(), _ordered_requests, ordered_it);
    update_timer();
    return true;
}

template <class V, class M, class I, class T>
void queue<V, M, I, T>::cancel(boost::system::error_code ec, time_traits::time_point expires_at) {
    if (ec) {
        return;
    }
    const lock_guard lock(_mutex);
    const auto begin = _expires_at_requests.begin();
    const auto end = _expires_at_requests.upper_bound(expires_at);
    std::for_each(begin, end, [&] (request_multimap_value& v) {
        const auto req = v.second;
        asio::post(*req->io_context, expired_handler(std::move(req->request)));
        _ordered_requests_pool.splice(_ordered_requests_pool.begin(), _ordered_requests, req->order_it);
    });
    _expires_at_requests.erase(_expires_at_requests.begin(), end);
    update_timer();
}

template <class V, class M, class I, class T>
void queue<V, M, I, T>::update_timer() {
    using timers_map_value = typename timers_map::value_type;
    if (_expires_at_requests.empty()) {
        std::for_each(_timers.begin(), _timers.end(), [] (timers_map_value& v) { v.second.cancel(); });
        _timers.clear();
        return;
    }
    const auto earliest_expire = _expires_at_requests.begin();
    const auto expires_at = earliest_expire->first;
    auto& timer = get_timer(*earliest_expire->second->io_context);
    timer.expires_at(expires_at);
    std::weak_ptr<queue> weak(this->shared_from_this());
    timer.async_wait([weak, expires_at] (boost::system::error_code ec) {
        if (const auto locked = weak.lock()) {
            locked->cancel(ec, expires_at);
        }
    });
}

template <class V, class M, class I, class T>
typename queue<V, M, I, T>::timer_t& queue<V, M, I, T>::get_timer(io_context_t& io_context) {
    auto it = _timers.find(&io_context);
    if (it != _timers.end()) {
        return it->second;
    }
    return _timers.emplace(&io_context, timer_t(io_context)).first->second;
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
