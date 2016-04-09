#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP

#include <list>
#include <map>

#include <boost/asio/steady_timer.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {

typedef boost::chrono::steady_clock clock;

template <class Value,
          class IoService = boost::asio::io_service,
          class Timer = boost::asio::basic_waitable_timer<clock> >
class queue : public boost::enable_shared_from_this<queue<Value, IoService, Timer> >,
    boost::noncopyable {
public:
    typedef Value value_type;
    typedef IoService io_service_t;
    typedef Timer timer_t;
    typedef boost::function<void ()> callback;
    typedef clock::duration time_duration;
    typedef clock::time_point time_point;

    queue(io_service_t& io_service,
          const boost::shared_ptr<timer_t>& timer,
          std::size_t capacity = 0)
            : _io_service(io_service), _timer(timer), _capacity(capacity) {}

    boost::shared_ptr<queue> shared_from_this() {
        return boost::enable_shared_from_this<queue>::shared_from_this();
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    bool empty() const;

    bool push(const value_type& req, const callback& req_expired,
              time_duration wait_duration);
    bool pop(value_type& req);

private:
    typedef boost::lock_guard<boost::mutex> lock_guard;
    struct expiring_request;
    typedef boost::shared_ptr<expiring_request> expiring_request_ptr;
    typedef std::list<expiring_request_ptr> request_list;
    typedef typename request_list::iterator request_list_it;
    typedef std::multimap<time_point, expiring_request_ptr> request_multimap;
    typedef typename request_multimap::iterator request_multimap_it;
    typedef typename request_multimap::value_type request_multimap_value;

    struct expiring_request {
        queue::value_type request;
        callback expired;
        request_list_it order_it;
        request_multimap_it expires_at_it;

        expiring_request(const queue::value_type& request, const callback& expired)
                : request(request), expired(expired) {}
    };

    mutable boost::mutex _mutex;
    request_list _ordered_requests;
    request_multimap _expires_at_requests;
    io_service_t& _io_service;
    boost::shared_ptr<timer_t> _timer;
    const std::size_t _capacity;

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
        time_duration wait_duration) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return false;
    }
    const expiring_request_ptr& req = boost::make_shared<expiring_request>(
        req_data, req_expired);
    req->order_it = _ordered_requests.insert(_ordered_requests.end(), req);
    req->expires_at_it = _expires_at_requests.insert(std::make_pair(
        clock::now() + wait_duration, req));
    update_timer();
    return true;
}

template <class V, class I, class T>
bool queue<V, I, T>::pop(value_type& value) {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return false;
    }
    const expiring_request_ptr req = _ordered_requests.front();
    _ordered_requests.pop_front();
    _expires_at_requests.erase(req->expires_at_it);
    update_timer();
    value = req->request;
    return true;
}

template <class V, class I, class T>
void queue<V, I, T>::cancel(const boost::system::error_code& ec) {
    if (ec) {
        return;
    }
    const lock_guard lock(_mutex);
    const request_multimap_it end = _expires_at_requests.upper_bound(
        _timer->expires_at());
    std::for_each(_expires_at_requests.begin(), end,
                  bind(&queue::cancel_one, this, _1));
    _expires_at_requests.erase(_expires_at_requests.begin(), end);
    update_timer();
}

template <class V, class I, class T>
void queue<V, I, T>::cancel_one(const request_multimap_value &pair) {
    const expiring_request_ptr& req = pair.second;
    _ordered_requests.erase(req->order_it);
    _io_service.post(req->expired);
}

template <class V, class I, class T>
void queue<V, I, T>::update_timer() {
    if (_expires_at_requests.empty()) {
        return;
    }
    const time_point& eariest_expires_at = _expires_at_requests.begin()->first;
    _timer->expires_at(eariest_expires_at);
    _timer->async_wait(bind(&queue::cancel, shared_from_this(), _1));
}

} // namespace detail
} // namespace async
} // namespace resource_pool
} // namespace yamail

#endif // YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_QUEUE_HPP
