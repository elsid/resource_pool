#include "tests.hpp"

#include <yamail/resource_pool/async/pool.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async;

using yamail::resource_pool::detail::pool_returns;

struct mocked_pool_impl : pool_returns<resource> {
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
    MOCK_METHOD3(get, void (mocked_io_context&, const callback&, time_traits::duration));
    MOCK_METHOD1(recycle, void (list_iterator));
    MOCK_METHOD1(waste, void (list_iterator));
    MOCK_METHOD0(disable, void ());
};

using resource_pool = pool<resource, std::mutex, mocked_io_context, StrictMock<mocked_pool_impl>>;

using boost::system::error_code;

struct async_resource_pool : Test {
    StrictMock<executor_gmock> executor;
    mocked_executor executor_wrapper {&executor};
    mocked_io_context io {&executor_wrapper};
    mocked_pool_impl::callback on_get;
    mocked_pool_impl::list resources;
    mocked_pool_impl::list_iterator resource_iterator;

    async_resource_pool()
        : resources(1), resource_iterator(resources.begin()) {}
};

TEST_F(async_resource_pool, call_capacity_should_call_impl_capacity) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    const resource_pool pool(pool_impl);

    InSequence s;

    EXPECT_CALL(*pool_impl, capacity()).WillOnce(Return(0));
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.capacity();
}

TEST_F(async_resource_pool, call_size_should_call_impl_size) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    const resource_pool pool(pool_impl);

    InSequence s;

    EXPECT_CALL(*pool_impl, size()).WillOnce(Return(0));
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.size();
}

TEST_F(async_resource_pool, call_available_should_call_impl_available) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    const resource_pool pool(pool_impl);

    InSequence s;

    EXPECT_CALL(*pool_impl, available()).WillOnce(Return(0));
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.available();
}

TEST_F(async_resource_pool, call_used_should_call_impl_used) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    const resource_pool pool(pool_impl);

    InSequence s;

    EXPECT_CALL(*pool_impl, used()).WillOnce(Return(0));
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.used();
}

TEST_F(async_resource_pool, call_stats_should_call_impl_stats) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    const resource_pool pool(pool_impl);

    InSequence s;

    EXPECT_CALL(*pool_impl, stats()).WillOnce(Return(async::stats {0, 0, 0, 0}));
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.stats();
}

TEST_F(async_resource_pool, move_than_dtor_should_call_disable_only_for_destination) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool src(pool_impl);

    EXPECT_CALL(*pool_impl, disable()).Times(0);

    const auto dst = std::move(src);

    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());
}

class check_error {
public:
    check_error(const error_code& error) : error(error) {}
    check_error(const error::code& error) : error(make_error_code(error)) {}

    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error);
        EXPECT_EQ(res.usable(), error == error_code());
    }

private:
    const error_code error;
};

struct check_no_error : check_error {
    check_no_error() : check_error(error_code()) {}
};

TEST_F(async_resource_pool, get_auto_recylce_handle_should_call_recycle) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, recycle(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_recycle(io, check_no_error());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_should_call_waste) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_waste(io, check_no_error());
    on_get(error_code(), resource_iterator);
}

struct recycle_resource {
    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_TRUE(res.usable());
        res.recycle();
    }
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_recycle_should_call_recycle_once) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, recycle(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_recycle(io, recycle_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_recycle_should_call_recycle_once) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, recycle(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_waste(io, recycle_resource());
    on_get(error_code(), resource_iterator);
}

struct waste_resource {
    void operator ()(const error_code& err, resource_pool::handle res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_TRUE(res.usable());
        res.waste();
    }
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_waste_should_call_waste_once) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_recycle(io, waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_waste_should_call_waste_once) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, waste(_)).WillOnce(Return());
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_waste(io, waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_from_pool_returns_error_should_not_call_waste_or_recycle) {
    const auto pool_impl = std::make_shared<StrictMock<mocked_pool_impl>>();
    resource_pool pool(pool_impl);

    EXPECT_CALL(*pool_impl, get(_, _, _)).WillOnce(SaveArg<1>(&on_get));
    EXPECT_CALL(*pool_impl, waste(_)).Times(0);
    EXPECT_CALL(*pool_impl, recycle(_)).Times(0);
    EXPECT_CALL(*pool_impl, disable()).WillOnce(Return());

    pool.get_auto_waste(io, check_error(error::get_resource_timeout));
    on_get(make_error_code(error::get_resource_timeout), mocked_pool_impl::list_iterator());
}

struct mocked_callback {
    MOCK_METHOD0(call, void ());
};

struct on_get_callback {
    using executor_type = mocked_executor;

    std::shared_ptr<mocked_callback> call;
    mocked_executor executor;

    void operator ()(const boost::system::error_code&, async::pool<resource>::handle) {
        call->call();
    }

    const executor_type& get_executor() const {
        return executor;
    }
};

TEST_F(async_resource_pool, asio_use_executor) {
    boost::asio::io_context service;
    async::pool<resource> pool(1, 0);
    const auto call = std::make_shared<mocked_callback>();

    const InSequence s;

    EXPECT_CALL(executor, on_work_started()).WillOnce(Return());
    EXPECT_CALL(executor, dispatch(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(*call, call()).WillOnce(Return());
    EXPECT_CALL(executor, on_work_finished()).WillOnce(Return());

    pool.get_auto_waste(service, on_get_callback {call, executor_wrapper});
    service.run();
}

}
