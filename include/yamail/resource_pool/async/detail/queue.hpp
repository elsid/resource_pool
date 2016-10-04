#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>

#include <list>
#include <map>
#include <mutex>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

typedef std::chrono::steady_clock clock;

template <class Value,
          class IoService = boost::asio::io_service,
          class Timer = time_traits::timer >
class queue : public std::enable_shared_from_this<queue<Value, IoService, Timer> >,
    boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Timer timer_t;
    typedef std::function<void ()> callback;

    queue(io_service_t& io_service, std::size_t capacity)
            : _io_service(io_service), _capacity(capacity), _timer(io_service) {}

    std::shared_ptr<queue> shared_from_this() {
        return std::enable_shared_from_this<queue>::shared_from_this();
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    bool empty() const;
    const timer_t& timer() const { return _timer; }

    bool push(const value_type& req, const callback& req_expired,
              time_traits::duration wait_duration);
    bool pop(value_type& req);

private:
    typedef std::lock_guard<std::mutex> lock_guard;

    struct expiring_request {
        typedef std::list<expiring_request> list;
        typedef typename list::iterator list_it;
        typedef std::multimap<time_traits::time_point, const expiring_request*> multimap;
        typedef typename multimap::iterator multimap_it;

        queue::value_type request;
        callback expired;
        list_it order_it;
        multimap_it expires_at_it;

        expiring_request(const queue::value_type& request, const callback& expired)
                : request(request), expired(expired) {}
    };

    typedef typename expiring_request::list_it request_list_it;
    typedef typename expiring_request::multimap_it request_multimap_it;
    typedef typename expiring_request::multimap::value_type request_multimap_value;

    mutable std::mutex _mutex;
    typename expiring_request::list _ordered_requests;
    typename expiring_request::multimap _expires_at_requests;
    io_service_t& _io_service;
    const std::size_t _capacity;
    timer_t _timer;

    bool fit_capacity() const { return _expires_at_requests.size() < _capacity; }
    void cancel(const boost::system::error_code& ec);
    void cancel_one(const request_multimap_value& pair);
    void update_timer();
};

template <class V, class I, class T>
std::size_t queue<V, I, T>::size() const {
    const lock_guard lock(_mutex);
    return _expires_at_requests.size();
}

template <class V, class I, class T>
bool queue<V, I, T>::empty() const {
    const lock_guard lock(_mutex);
    return _ordered_requests.empty();
}

template <class V, class I, class T>
bool queue<V, I, T>::push(const value_type& req_data, const callback& req_expired,
        time_traits::duration wait_duration) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return false;
    }
    request_list_it order_it = _ordered_requests.insert(
        _ordered_requests.end(), expiring_request(req_data, req_expired));
    expiring_request& req = *order_it;
    req.order_it = order_it;
    req.expires_at_it = _expires_at_requests.insert(
        std::make_pair(time_traits::add(time_traits::now(), wait_duration), &req));
    update_timer();
    return true;
}

template <class V, class I, class T>
bool queue<V, I, T>::pop(value_type& value) {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return false;
    }
    const expiring_request& req = _ordered_requests.front();
    value = req.request;
    _expires_at_requests.erase(req.expires_at_it);
    _ordered_requests.pop_front();
    update_timer();
    return true;
}

template <class V, class I, class T>
void queue<V, I, T>::cancel(const boost::system::error_code& ec) {
    if (ec) {
        return;
    }
    const lock_guard lock(_mutex);
    const request_multimap_it begin = _expires_at_requests.begin();
    const request_multimap_it end = _expires_at_requests.upper_bound(
        _timer.expires_at());
    for (request_multimap_it it = begin; it != end; ++it) {
        cancel_one(*it);
    }
    _expires_at_requests.erase(_expires_at_requests.begin(), end);
    update_timer();
}

template <class V, class I, class T>
void queue<V, I, T>::cancel_one(const request_multimap_value &pair) {
    const expiring_request* req = pair.second;
    _io_service.post(req->expired);
    _ordered_requests.erase(req->order_it);
}

template <class V, class I, class T>
void queue<V, I, T>::update_timer() {
    if (_expires_at_requests.empty()) {
        return;
    }
    const time_traits::time_point& eariest_expires_at = _expires_at_requests.begin()->first;
    _timer.expires_at(eariest_expires_at);
    std::weak_ptr<queue> weak(shared_from_this());
    _timer.async_wait([weak] (const boost::system::error_code& ec) {
        if (const auto locked = weak.lock()) {
            locked->cancel(ec);
        }
    });
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
