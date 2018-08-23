#ifndef YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
#define YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP

#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>

#include <boost/asio/is_executor.hpp>
#include <boost/asio/executor.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <list>

namespace tests {

namespace asio = boost::asio;

using namespace testing;
using namespace yamail::resource_pool;

struct resource {
    resource() = default;
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource&) = delete;
    resource& operator =(resource&&) = default;
};

struct request {
    int value;
};

struct executor_mock {
    virtual ~executor_mock() = default;
    virtual void on_work_started() const = 0;
    virtual void on_work_finished() const = 0;
    virtual void dispatch(std::function<void ()>) const = 0;
    virtual void post(std::function<void ()>) const = 0;
    virtual void defer(std::function<void ()>) const = 0;
};

struct executor_gmock : executor_mock {
    MOCK_CONST_METHOD0(on_work_started, void ());
    MOCK_CONST_METHOD0(on_work_finished, void ());
    MOCK_CONST_METHOD1(dispatch, void (std::function<void ()>));
    MOCK_CONST_METHOD1(post, void (std::function<void ()>));
    MOCK_CONST_METHOD1(defer, void (std::function<void ()>));
};

template <class Handler>
struct shared_wrapper {
    std::shared_ptr<Handler> ptr;

    template <class ... Args>
    void operator ()(Args&& ... args) {
        return (*ptr)(std::forward<Args>(args) ...);
    }
};

template <class Function>
auto wrap_shared(Function&& f) {
    return shared_wrapper<std::decay_t<Function>> {std::make_shared<std::decay_t<Function>>(std::forward<Function>(f))};
}

struct mocked_executor {
    const executor_mock* impl = nullptr;
    asio::execution_context* context_ = nullptr;

    asio::execution_context& context() noexcept {
        return *context_;
    }

    void on_work_started() const {
        return impl->on_work_started();
    }

    void on_work_finished() const {
        return impl->on_work_finished();
    }

    template <class Function>
    void dispatch(Function&& f, std::allocator<void>) const {
        return impl->dispatch(wrap_shared(std::forward<Function>(f)));
    }

    template <class Function>
    void post(Function&& f, std::allocator<void>) const {
        return impl->post(wrap_shared(std::forward<Function>(f)));
    }

    template <class Function>
    void defer(Function&& f, std::allocator<void>) const {
        return impl->defer(wrap_shared(std::forward<Function>(f)));
    }

    friend bool operator ==(const mocked_executor& lhs, const mocked_executor& rhs) {
        return lhs.context_ == rhs.context_ && lhs.impl == rhs.impl;
    }
};

struct mocked_io_context : asio::execution_context {
    using executor_type = mocked_executor;

    executor_type* executor = nullptr;

    mocked_io_context(executor_type* executor)
        : executor(executor) {}

    executor_type get_executor() const {
        return *executor;
    }
};

} // namespace tests

namespace boost {
namespace asio {

template <>
struct is_executor<tests::mocked_executor> : true_type {};

} // namespace asio
} // namespace boost

#endif // YAMAIL_RESOURCE_POOL_TEST_ASYNC_TESTS_HPP
