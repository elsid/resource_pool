#include "tests.hpp"

#include <yamail/resource_pool/async/pool.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async;

struct mocked_pool_impl {
    using value_type = resource;
    using idle = yamail::resource_pool::detail::idle<value_type>;
    using list = std::list<idle>;
    using list_iterator = list::iterator;
    using callback = std::function<void (const boost::system::error_code&, list_iterator)>;

    MOCK_CONST_METHOD0(capacity, std::size_t ());
    MOCK_CONST_METHOD0(size, std::size_t ());
    MOCK_CONST_METHOD0(available, std::size_t ());
    MOCK_CONST_METHOD0(used, std::size_t ());
    MOCK_CONST_METHOD0(stats, async::stats ());
    MOCK_CONST_METHOD3(get, void (mocked_io_context&, const callback&, time_traits::duration));
    MOCK_CONST_METHOD1(recycle, void (list_iterator));
    MOCK_CONST_METHOD1(waste, void (list_iterator));
    MOCK_CONST_METHOD0(disable, void ());

    mocked_pool_impl(std::size_t, std::size_t, time_traits::duration) {}
};

using resource_pool = pool<resource, std::mutex, mocked_io_context, mocked_pool_impl>;

using boost::system::error_code;

struct async_resource_pool : Test {
    mocked_io_context io;
    mocked_pool_impl::callback on_get;
    mocked_pool_impl::list resources;
    mocked_pool_impl::list_iterator resource_iterator;

    async_resource_pool()
        : resources(1), resource_iterator(resources.begin()) {}
};

TEST_F(async_resource_pool, create_without_mocks_should_succeed) {
    boost::asio::io_context io;
    pool<resource>(1, 1);
}

TEST_F(async_resource_pool, call_capacity_should_call_impl_capacity) {
    const resource_pool pool(0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), capacity()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.capacity();
}

TEST_F(async_resource_pool, call_size_should_call_impl_size) {
    const resource_pool pool(0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), size()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.size();
}

TEST_F(async_resource_pool, call_available_should_call_impl_available) {
    const resource_pool pool(0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), available()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.available();
}

TEST_F(async_resource_pool, call_used_should_call_impl_used) {
    const resource_pool pool(0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), used()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.used();
}

TEST_F(async_resource_pool, call_stats_should_call_impl_stats) {
    const resource_pool pool(0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), stats()).WillOnce(Return(async::stats {0, 0, 0, 0}));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.stats();
}

TEST_F(async_resource_pool, move_than_dtor_should_call_disable_only_for_destination) {
    resource_pool src(0, 0);

    EXPECT_CALL(src.impl(), disable()).Times(0);

    const auto dst = std::move(src);

    EXPECT_CALL(dst.impl(), disable()).WillOnce(Return());
}

class check_error {
public:
    check_error(const error_code& error) : error(error) {}
    check_error(const error::code& error) : error(make_error_code(error)) {}

    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error);
        EXPECT_EQ(res.unusable(), error != error_code());
    }

private:
    const error_code error;
};

struct check_no_error : check_error {
    check_no_error() : check_error(error_code()) {}
};

TEST_F(async_resource_pool, get_auto_recylce_handle_should_call_recycle) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(io, check_no_error());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_should_call_waste) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(io, check_no_error());
    on_get(error_code(), resource_iterator);
}

struct recycle_resource {
    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res.unusable());
        res.recycle();
    }
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(io, recycle_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(io, recycle_resource());
    on_get(error_code(), resource_iterator);
}

struct waste_resource {
    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res.unusable());
        res.waste();
    }
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_waste_should_call_waste_once) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(io, waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_waste_should_call_waste_once) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(io, waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_from_pool_returns_error_should_not_call_waste_or_recycle) {
    resource_pool pool(0, 0);

    EXPECT_CALL(pool.impl(), get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).Times(0);
    EXPECT_CALL(pool.impl(), recycle(_)).Times(0);
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(io, check_error(error::get_resource_timeout));
    on_get(make_error_code(error::get_resource_timeout), mocked_pool_impl::list_iterator());
}

struct mocked_callback {
    MOCK_METHOD0(call, void ());
    MOCK_METHOD0(asio_handler_invoke, void ());
};

struct on_get_callback {
    std::shared_ptr<mocked_callback> call;

    void operator ()(const boost::system::error_code&, async::pool<resource>::handle) {
        call->call();
    }

    template <class Function>
    friend void asio_handler_invoke(Function function, on_get_callback* handler) {
        using boost::asio::asio_handler_invoke;
        handler->call->asio_handler_invoke();
        function();
    }
};

TEST_F(async_resource_pool, asio_handler_invoke) {
    boost::asio::io_context service;
    async::pool<resource> pool(1, 0);
    const auto call = std::make_shared<mocked_callback>();

    const InSequence s;

    EXPECT_CALL(*call, asio_handler_invoke()).WillOnce(Return());
    EXPECT_CALL(*call, call()).WillOnce(Return());

    pool.get_auto_waste(service, on_get_callback {call});
    service.run();
}

}
