#include "tests.hpp"

#include <yamail/resource_pool/async/pool.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async;

struct mocked_pool_impl {
    typedef resource value_type;
    typedef std::shared_ptr<value_type> pointer;
    typedef yamail::resource_pool::detail::idle<pointer> idle;
    typedef std::list<idle> list;
    typedef list::iterator list_iterator;
    typedef std::function<void (const boost::system::error_code&, list_iterator)> callback;
    typedef std::function<void (const boost::system::error_code&)> on_catch_handler_exception_type;

    MOCK_CONST_METHOD0(capacity, std::size_t ());
    MOCK_CONST_METHOD0(size, std::size_t ());
    MOCK_CONST_METHOD0(available, std::size_t ());
    MOCK_CONST_METHOD0(used, std::size_t ());
    MOCK_CONST_METHOD2(get, void (const callback&, time_traits::duration));
    MOCK_CONST_METHOD1(recycle, void (list_iterator));
    MOCK_CONST_METHOD1(waste, void (list_iterator));
    MOCK_CONST_METHOD0(disable, void ());

    mocked_pool_impl(mocked_io_service&, std::size_t, std::size_t, time_traits::duration,
                     const on_catch_handler_exception_type&) {}
};

typedef pool<resource, mocked_io_service, mocked_pool_impl> resource_pool;

using boost::system::error_code;

struct async_resource_pool : Test {
    mocked_io_service ios;
    mocked_pool_impl::callback on_get;
    mocked_pool_impl::list resources;
    mocked_pool_impl::list_iterator resource_iterator;

    async_resource_pool()
        : resources(1), resource_iterator(resources.begin()) {}
};

TEST_F(async_resource_pool, create_without_mocks_should_succeed) {
    boost::asio::io_service ios;
    pool<resource>(ios, 1, 1);
}

TEST_F(async_resource_pool, call_capacity_should_call_impl_capacity) {
    resource_pool pool(ios, 0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), capacity()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.capacity();
}

TEST_F(async_resource_pool, call_size_should_call_impl_size) {
    resource_pool pool(ios, 0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), size()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.size();
}

TEST_F(async_resource_pool, call_available_should_call_impl_available) {
    resource_pool pool(ios, 0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), available()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.available();
}

TEST_F(async_resource_pool, call_used_should_call_impl_used) {
    resource_pool pool(ios, 0, 0);

    InSequence s;

    EXPECT_CALL(pool.impl(), used()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.used();
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
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(check_no_error());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_should_call_waste) {
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(check_no_error());
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
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(recycle_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(recycle_resource());
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
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_recycle(waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_waste_should_call_waste_once) {
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(waste_resource());
    on_get(error_code(), resource_iterator);
}

TEST_F(async_resource_pool, get_from_pool_returns_error_should_not_call_waste_or_recycle) {
    resource_pool pool(ios, 0, 0);

    EXPECT_CALL(pool.impl(), get(_, _)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.impl(), waste(_)).Times(0);
    EXPECT_CALL(pool.impl(), recycle(_)).Times(0);
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.get_auto_waste(check_error(error::get_resource_timeout));
    on_get(make_error_code(error::get_resource_timeout), mocked_pool_impl::list_iterator());
}

}
