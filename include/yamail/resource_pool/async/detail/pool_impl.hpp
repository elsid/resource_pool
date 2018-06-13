#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/detail/idle.hpp>
#include <yamail/resource_pool/async/detail/queue.hpp>
#include <boost/asio/spawn.hpp>
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

template <class Value, class Mutex, class IoService, class Queue>
class pool_impl : boost::noncopyable {
public:
    using value_type = Value;
    using io_service_t = IoService;
    using idle = resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = typename list::iterator;
    using callback = std::function<void (const boost::system::error_code&, list_iterator)>;
    using queue_type = Queue;

    pool_impl(std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {

        for (std::size_t i = 0; i < _capacity; ++i) {
            _wasted.emplace_back(idle());
        }
    }

    template <class Generator>
    pool_impl(Generator&& gen_value,
              std::size_t capacity,
              std::size_t queue_capacity,
              time_traits::duration idle_timeout)
            : _capacity(assert_capacity(capacity)),
              _idle_timeout(idle_timeout),
              _callbacks(std::make_shared<queue_type>(queue_capacity)) {

        const auto drop_time = time_traits::add(time_traits::now(), _idle_timeout);
        for (std::size_t i = 0; i < _capacity; ++i) {
            _available.emplace_back(gen_value(), drop_time);
        }
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

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    std::size_t available() const;
    std::size_t used() const;
    async::stats stats() const;

    const queue_type& queue() const { return *_callbacks; }

    template <class Callback>
    void get(io_service_t& io_service, Callback call, time_traits::duration wait_duration = time_traits::duration(0));
    void recycle(list_iterator res_it);
    void waste(list_iterator res_it);
    void disable();

    static std::size_t assert_capacity(std::size_t value);

private:
    using mutex_t = Mutex;
    using unique_lock = std::unique_lock<mutex_t>;
    using lock_guard = std::lock_guard<mutex_t>;

    mutable mutex_t _mutex;
    list _available;
    list _used;
    list _wasted;
    const std::size_t _capacity;
    const time_traits::duration _idle_timeout;
    std::shared_ptr<queue_type> _callbacks;
    bool _disabled = false;

    std::size_t size_unsafe() const { return _available.size() + _used.size(); }
    bool fit_capacity() const { return size_unsafe() < _capacity; }
    template <class Callback>
    bool reserve_resource(io_service_t& io_service, unique_lock& lock, Callback call);
    template <class Callback>
    bool alloc_resource(io_service_t& io_service, unique_lock& lock, Callback call);
    template <class Serve>
    void perform_one_request(unique_lock& lock, Serve&& serve);
};

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::size() const {
    const lock_guard lock(_mutex);
    return size_unsafe();
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::available() const {
    const lock_guard lock(_mutex);
    return _available.size();
}

template <class V, class M, class I, class Q>
std::size_t pool_impl<V, M, I, Q>::used() const {
    const lock_guard lock(_mutex);
    return _used.size();
}

template <class V, class M, class I, class Q>
async::stats pool_impl<V, M, I, Q>::stats() const {
    const lock_guard lock(_mutex);
    return async::stats {size_unsafe(), _available.size(), _used.size(), _callbacks->size()};
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::recycle(list_iterator res_it) {
    res_it->drop_time = time_traits::add(time_traits::now(), _idle_timeout);
    unique_lock lock(_mutex);
    _available.splice(_available.end(), _used, res_it);
    perform_one_request(lock, [&] (io_service_t& ios, const callback& call) { return alloc_resource(ios, lock, call); });
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::waste(list_iterator res_it) {
    res_it->value.reset();
    unique_lock lock(_mutex);
    _wasted.splice(_wasted.end(), _used, res_it);
    perform_one_request(lock, [&] (io_service_t& ios, const callback& call) { return reserve_resource(ios, lock, call); });
}

template <class V, class M, class I, class Q>
template <class Callback>
void pool_impl<V, M, I, Q>::get(io_service_t& io_service, Callback call, time_traits::duration wait_duration) {
    using boost::asio::asio_handler_invoke;
    unique_lock lock(_mutex);
    if (_disabled) {
        lock.unlock();
        io_service.post([call] () mutable {
            asio_handler_invoke([=] () mutable { call(make_error_code(error::disabled), list_iterator()); }, &call);
        });
    } else if (alloc_resource(io_service, lock, call)) {
        return;
    } else if (fit_capacity()) {
        reserve_resource(io_service, lock, call);
    } else {
        lock.unlock();
        if (wait_duration.count() == 0) {
            io_service.post([call] () mutable {
                asio_handler_invoke([=] () mutable { call(make_error_code(error::get_resource_timeout), list_iterator()); }, &call);
            });
        } else {
            const auto expired = [call] () mutable {
                asio_handler_invoke([=] () mutable { call(make_error_code(error::get_resource_timeout), list_iterator()); }, &call);
            };
            const bool pushed = _callbacks->push(io_service, call, expired, wait_duration);
            if (!pushed) {
                io_service.post([call] () mutable {
                    asio_handler_invoke([=] () mutable { call(make_error_code(error::request_queue_overflow), list_iterator()); }, &call);
                });
            }
        }
    }
}

template <class V, class M, class I, class Q>
void pool_impl<V, M, I, Q>::disable() {
    using boost::asio::asio_handler_invoke;
    const lock_guard lock(_mutex);
    _disabled = true;
    while (true) {
        io_service_t* io_service;
        callback call;
        if (!_callbacks->pop(io_service, call)) {
            break;
        }
        io_service->post([call] {
            asio_handler_invoke([=] () mutable { call(make_error_code(error::disabled), list_iterator()); }, &call);
        });
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
template <class Callback>
bool pool_impl<V, M, I, Q>::alloc_resource(io_service_t& io_service, unique_lock& lock, Callback call) {
    using boost::asio::asio_handler_invoke;
    while (!_available.empty()) {
        const list_iterator res_it = _available.begin();
        if (res_it->drop_time <= time_traits::now()) {
            res_it->value.reset();
            _wasted.splice(_wasted.end(), _available, res_it);
            continue;
        }
        _used.splice(_used.end(), _available, res_it);
        lock.unlock();
        io_service.post([call, res_it] () mutable {
            asio_handler_invoke([=] () mutable { call(boost::system::error_code(), res_it); }, &call);
        });
        return true;
    }
    return false;
}

template <class V, class M, class I, class Q>
template <class Callback>
bool pool_impl<V, M, I, Q>::reserve_resource(io_service_t& io_service, unique_lock& lock, Callback call) {
    using boost::asio::asio_handler_invoke;
    const list_iterator res_it = _wasted.begin();
    _used.splice(_used.end(), _wasted, res_it);
    lock.unlock();
    io_service.post([call, res_it] () mutable {
        asio_handler_invoke([=] () mutable { call(boost::system::error_code(), res_it); }, &call);
    });
    return true;
}

template <class V, class M, class I, class Q>
template <class Serve>
void pool_impl<V, M, I, Q>::perform_one_request(unique_lock& lock, Serve&& serve) {
    io_service_t* io_service;
    callback call;
    if (_callbacks->pop(io_service, call)) {
        if (!serve(*io_service, call)) {
            reserve_resource(*io_service, lock, call);
        }
    }
}

using boost::asio::async_result;
using boost::asio::handler_type;

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
using async_return_type = typename ::boost::asio::async_result<
        typename handler_type<Handler, Signature>::type
    >::type;

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_POOL_IMPL_HPP
