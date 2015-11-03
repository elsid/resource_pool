#ifndef YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_REQUEST_QUEUE_QUEUE_HPP
#define YAMAIL_RESOURCE_POOL_ASYNC_DETAIL_REQUEST_QUEUE_QUEUE_HPP

#include <list>
#include <map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/make_shared.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>

#include <yamail/resource_pool/error.hpp>

namespace yamail {
namespace resource_pool {
namespace async {
namespace detail {
namespace request_queue {

typedef boost::chrono::steady_clock clock;

template <class T>
class queue : public boost::enable_shared_from_this<queue<T> >,
    boost::noncopyable {
public:
    typedef T value_type;
    typedef boost::function<void ()> callback;
    typedef clock::duration time_duration;
    typedef clock::time_point time_point;
    typedef boost::optional<value_type> value_type_opt;

    struct pop_result {
        boost::system::error_code error;
        value_type_opt request;

        pop_result(const boost::system::error_code& error) : error(error) {}
        pop_result(const queue::value_type& request) : request(request) {}

        bool operator ==(const boost::system::error_code& error) const {
            return this->error == error;
        }
    };

    queue(boost::asio::io_service& io_service, std::size_t capacity = 0)
            : _io_service(io_service), _capacity(capacity), _timer(io_service) {}

    boost::shared_ptr<queue> shared_from_this() {
        return boost::enable_shared_from_this<queue>::shared_from_this();
    }

    std::size_t capacity() const { return _capacity; }
    std::size_t size() const;
    bool empty() const;

    boost::system::error_code push(value_type req, callback req_expired,
            const time_duration& wait_duration);
    pop_result pop();

private:
    typedef boost::lock_guard<boost::mutex> lock_guard;
    typedef boost::asio::basic_waitable_timer<clock> timer;
    struct expiring_request;
    typedef boost::shared_ptr<expiring_request> expiring_request_ptr;
    typedef std::list<expiring_request_ptr> request_list;
    typedef typename request_list::iterator request_list_it;
    typedef std::multimap<time_point, expiring_request_ptr> request_multimap;
    typedef typename request_multimap::iterator request_multimap_it;
    typedef typename std::pair<request_multimap_it, request_multimap_it> request_multimap_range;
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
    boost::asio::io_service& _io_service;
    const std::size_t _capacity;
    timer _timer;

    bool fit_capacity() const { return _expires_at_requests.size() < _capacity; }
    void cancel(const boost::system::error_code& ec);
    void cancel_one(const request_multimap_value& pair);
    void update_timer();
};

template <class T>
std::size_t queue<T>::size() const {
    const lock_guard lock(_mutex);
    return _expires_at_requests.size();
}

template <class T>
bool queue<T>::empty() const {
    const lock_guard lock(_mutex);
    return _ordered_requests.empty();
}

template <class T>
boost::system::error_code queue<T>::push(value_type req_data, callback req_expired,
        const time_duration& wait_duration) {
    const lock_guard lock(_mutex);
    if (!fit_capacity()) {
        return make_error_code(error::request_queue_overflow);
    }
    const expiring_request_ptr req = boost::make_shared<expiring_request>(
        req_data, req_expired);
    req->order_it = _ordered_requests.insert(_ordered_requests.end(), req);
    req->expires_at_it = _expires_at_requests.insert(std::make_pair(
        clock::now() + wait_duration, req));
    update_timer();
    return boost::system::error_code();
}

template <class T>
typename queue<T>::pop_result queue<T>::pop() {
    const lock_guard lock(_mutex);
    if (_ordered_requests.empty()) {
        return make_error_code(error::request_queue_is_empty);
    }
    const expiring_request_ptr req = _ordered_requests.front();
    _ordered_requests.pop_front();
    _expires_at_requests.erase(req->expires_at_it);
    update_timer();
    return req->request;
}

template <class T>
void queue<T>::cancel(const boost::system::error_code& ec) {
    if (ec != boost::system::errc::success) {
        return;
    }
    const lock_guard lock(_mutex);
    const request_multimap_range& range = _expires_at_requests.equal_range(
        _timer.expires_at());
    boost::for_each(range, bind(&queue::cancel_one, this, _1));
    _expires_at_requests.erase(range.first, range.second);
    update_timer();
}

template <class T>
void queue<T>::cancel_one(const request_multimap_value &pair) {
    const expiring_request_ptr req = pair.second;
    _ordered_requests.erase(req->order_it);
    _io_service.post(req->expired);
}

template <class T>
void queue<T>::update_timer() {
    if (_expires_at_requests.empty()) {
        return;
    }
    const time_point& eariest_expires_at = _expires_at_requests.begin()->first;
    _timer.expires_at(eariest_expires_at);
    _timer.async_wait(bind(&queue::cancel, shared_from_this(), _1));
}

}
}
}
}
}

#endif