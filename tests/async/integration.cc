#include <yamail/resource_pool/async/pool.hpp>

#include <boost/asio/dispatch.hpp>

#include <gtest/gtest.h>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async;

namespace asio = boost::asio;

struct resource {
    int value;

    resource(int value) : value(value) {}
    resource(const resource&) = delete;
    resource(resource&&) = default;
    resource& operator =(const resource&) = delete;
    resource& operator =(resource&&) = default;

    friend bool operator ==(const resource& lhs, const resource& rhs) {
        return lhs.value == rhs.value;
    }
};

using resource_pool = pool<resource>;
using boost::system::error_code;

struct async_resource_pool_integration : Test {
    asio::io_context io;
    std::atomic_flag coroutine_finished = ATOMIC_FLAG_INIT;
    std::atomic_flag coroutine1_finished = ATOMIC_FLAG_INIT;
    std::atomic_flag coroutine2_finished = ATOMIC_FLAG_INIT;
    std::atomic_flag coroutine3_finished = ATOMIC_FLAG_INIT;
    std::atomic_flag on_get_called = ATOMIC_FLAG_INIT;
    std::atomic_flag on_get1_called = ATOMIC_FLAG_INIT;
    std::atomic_flag on_get2_called = ATOMIC_FLAG_INIT;
    std::atomic_flag on_get3_called = ATOMIC_FLAG_INIT;
};

TEST_F(async_resource_pool_integration, first_get_auto_recycle_should_return_usable_empty_handle_to_resource) {
    resource_pool pool(1, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, after_get_auto_recycle_pool_should_save_handle_state) {
    resource_pool pool(1, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        {
            auto handle = pool.get_auto_recycle(io, yield);
            ASSERT_FALSE(handle.unusable());
            EXPECT_TRUE(handle.empty());
            handle.reset(resource {42});
        }
        {
            const auto handle = pool.get_auto_recycle(io, yield);
            EXPECT_FALSE(handle.unusable());
            ASSERT_FALSE(handle.empty());
            EXPECT_EQ(*handle, resource {42});
        }

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, parallel_requests_should_get_different_handles) {
    resource_pool pool(2, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {42});

        ASSERT_FALSE(coroutine1_finished.test_and_set());
    });

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {13});

        ASSERT_FALSE(coroutine2_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine1_finished.test_and_set());
    EXPECT_TRUE(coroutine2_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, sequenced_requests_should_get_different_handles) {
    resource_pool pool(2, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        const auto handle1 = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle1.unusable());
        EXPECT_TRUE(handle1.empty());

        const auto handle2 = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle2.unusable());
        EXPECT_TRUE(handle2.empty());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, request_with_zero_wait_duration_should_not_be_pending) {
    resource_pool pool(1, 1);

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle1 = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle1.unusable());
        EXPECT_TRUE(handle1.empty());

        error_code ec;
        auto handle2 = pool.get_auto_recycle(io, yield[ec], time_traits::duration(0));
        EXPECT_EQ(ec, error_code(error::get_resource_timeout));
        EXPECT_TRUE(handle2.unusable());
        EXPECT_TRUE(handle2.empty());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, queue_should_store_pending_requests) {
    resource_pool pool(1, 1);

    const auto on_get = [&] (error_code ec, auto handle) {
        ASSERT_FALSE(on_get_called.test_and_set());

        EXPECT_FALSE(ec);
        EXPECT_FALSE(handle.unusable());
        ASSERT_FALSE(handle.empty());
        EXPECT_EQ(*handle, resource {42});
    };

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_recycle(io, yield);
        ASSERT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {42});

        pool.get_auto_recycle(io, on_get, time_traits::duration::max());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(on_get_called.test_and_set());
    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, for_zero_queue_capacity_should_not_be_pending_requests) {
    resource_pool pool(1, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle1 = pool.get_auto_recycle(io, yield);
        EXPECT_FALSE(handle1.unusable());
        EXPECT_TRUE(handle1.empty());

        error_code ec;
        auto handle2 = pool.get_auto_recycle(io, yield[ec], time_traits::duration::max());
        EXPECT_EQ(ec, error_code(error::request_queue_overflow));
        EXPECT_TRUE(handle2.unusable());
        EXPECT_TRUE(handle2.empty());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, recursive_get_auto_recycle_should_not_lead_to_locked_resources_for_all_calls) {
    resource_pool pool(2, 0);

    const auto on_get3 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get3_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);
    };

    const auto on_get2 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get2_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);

        pool.get_auto_recycle(io, on_get3);
    };

    const auto on_get1 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get1_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);

        pool.get_auto_recycle(io, on_get2);
    };

    asio::dispatch([&] {
        pool.get_auto_recycle(io, on_get1);
    });

    io.run();

    EXPECT_TRUE(on_get1_called.test_and_set());
    EXPECT_TRUE(on_get2_called.test_and_set());
    EXPECT_TRUE(on_get3_called.test_and_set());
}

TEST_F(async_resource_pool_integration, first_get_auto_waste_should_return_usable_empty_handle_to_resource) {
    resource_pool pool(1, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_waste(io, yield);
        EXPECT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, after_get_auto_waste_pool_should_reset_handle_state) {
    resource_pool pool(1, 0);

    asio::spawn(io, [&] (asio::yield_context yield) {
        {
            auto handle = pool.get_auto_waste(io, yield);
            ASSERT_FALSE(handle.unusable());
            EXPECT_TRUE(handle.empty());
            handle.reset(resource {42});
        }
        {
            const auto handle = pool.get_auto_waste(io, yield);
            EXPECT_FALSE(handle.unusable());
            EXPECT_TRUE(handle.empty());
        }

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, disabled_pool_should_cancel_all_pending_requests) {
    auto pool = std::make_unique<resource_pool>(1, 1);

    const auto on_get = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get_called.test_and_set());

        EXPECT_EQ(ec, error_code(error::disabled));
    };

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool->get_auto_recycle(io, yield);
        ASSERT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {42});

        pool->get_auto_recycle(io, on_get, time_traits::duration::max());
        pool.reset();

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(on_get_called.test_and_set());
    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, retries_to_get_resource_should_not_lead_to_infinite_timeout_errors) {
    resource_pool pool(1, 1);

    const auto on_get3 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get3_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);
    };

    const auto on_get2 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get2_called.test_and_set());

        EXPECT_EQ(ec, error_code(error::get_resource_timeout));
        ASSERT_EQ(pool.used(), 0u);

        pool.get_auto_recycle(io, on_get3);
    };

    const auto on_get1 = [&] (error_code ec, auto handle) {
        ASSERT_FALSE(on_get1_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);

        pool.get_auto_recycle(io, on_get2);
        handle.recycle();
    };

    asio::dispatch([&] {
        pool.get_auto_recycle(io, on_get1);
    });

    io.run();

    EXPECT_TRUE(on_get1_called.test_and_set());
    EXPECT_TRUE(on_get2_called.test_and_set());
    EXPECT_TRUE(on_get3_called.test_and_set());
}

TEST_F(async_resource_pool_integration, retries_to_get_resource_should_not_lead_to_infinite_queue_overflow_errors) {
    resource_pool pool(1, 0);

    const auto on_get3 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get3_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);
    };

    const auto on_get2 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get2_called.test_and_set());

        EXPECT_EQ(ec, error_code(error::request_queue_overflow));
        ASSERT_EQ(pool.used(), 0u);

        pool.get_auto_recycle(io, on_get3);
    };

    const auto on_get1 = [&] (error_code ec, auto handle) {
        ASSERT_FALSE(on_get1_called.test_and_set());

        EXPECT_FALSE(ec);
        ASSERT_EQ(pool.used(), 1u);

        pool.get_auto_recycle(io, on_get2, time_traits::duration::max());
        handle.recycle();
    };

    asio::dispatch([&] {
        pool.get_auto_recycle(io, on_get1);
    });

    io.run();

    EXPECT_TRUE(on_get1_called.test_and_set());
    EXPECT_TRUE(on_get2_called.test_and_set());
    EXPECT_TRUE(on_get3_called.test_and_set());
}

TEST_F(async_resource_pool_integration, enqueue_pending_request_on_timeout_should_not_lead_to_deadlock) {
    resource_pool pool(1, 1);
    std::atomic_bool finish_coroutine {false};

    const auto on_get2 = [&] (error_code ec, auto handle) {
        ASSERT_FALSE(on_get2_called.test_and_set());

        EXPECT_FALSE(ec);
        EXPECT_FALSE(handle.unusable());
        ASSERT_FALSE(handle.empty());
        EXPECT_EQ(*handle, resource {42});
    };

    const auto on_get1 = [&] (error_code ec, auto) {
        ASSERT_FALSE(on_get1_called.test_and_set());

        finish_coroutine = true;

        EXPECT_EQ(ec, error_code(error::get_resource_timeout));
        ASSERT_EQ(pool.used(), 1u);

        pool.get_auto_recycle(io, on_get2, time_traits::duration::max());
    };

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_recycle(io, yield);
        ASSERT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {42});

        pool.get_auto_recycle(io, on_get1, std::chrono::nanoseconds(1));

        while (!finish_coroutine) {
            asio::post(io, yield);
        }

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(on_get1_called.test_and_set());
    EXPECT_TRUE(on_get2_called.test_and_set());
    EXPECT_TRUE(coroutine_finished.test_and_set());
}

TEST_F(async_resource_pool_integration, pending_request_should_get_empty_handle_after_waste) {
    resource_pool pool(1, 1);

    const auto on_get = [&] (error_code ec, auto handle) {
        ASSERT_FALSE(on_get_called.test_and_set());

        EXPECT_FALSE(ec);
        EXPECT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
    };

    asio::spawn(io, [&] (asio::yield_context yield) {
        auto handle = pool.get_auto_waste(io, yield);
        ASSERT_FALSE(handle.unusable());
        EXPECT_TRUE(handle.empty());
        handle.reset(resource {42});

        pool.get_auto_recycle(io, on_get, time_traits::duration::max());

        ASSERT_FALSE(coroutine_finished.test_and_set());
    });

    io.run();

    EXPECT_TRUE(on_get_called.test_and_set());
    EXPECT_TRUE(coroutine_finished.test_and_set());
}

struct move_only_handler {
    std::atomic_flag* called = nullptr;

    move_only_handler() = default;
    move_only_handler(std::atomic_flag* called) : called(called) {}
    move_only_handler(const move_only_handler&) = delete;
    move_only_handler(move_only_handler&&) = default;

    void operator ()(error_code, resource_pool::handle) {
        ASSERT_FALSE(called->test_and_set());
    }
};

TEST_F(async_resource_pool_integration, get_auto_recycle_should_support_move_only_handler) {
    resource_pool pool(1, 1);
    pool.get_auto_recycle(io, move_only_handler(std::addressof(on_get_called)));
    io.run();
    EXPECT_TRUE(on_get_called.test_and_set());
}

TEST_F(async_resource_pool_integration, get_auto_waste_should_support_move_only_handler) {
    resource_pool pool(1, 1);
    pool.get_auto_waste(io, move_only_handler(std::addressof(on_get_called)));
    io.run();
    EXPECT_TRUE(on_get_called.test_and_set());
}

}
