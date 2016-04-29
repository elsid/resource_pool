#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <yamail/resource_pool/sync/pool.hpp>

namespace {

using namespace testing;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::sync;

using boost::make_shared;

struct resource {};

struct mocked_pool_impl {
    typedef resource value_type;
    typedef boost::shared_ptr<value_type> pointer;
    typedef yamail::resource_pool::detail::idle<pointer> idle;
    typedef std::list<idle> list;
    typedef list::iterator list_iterator;
    typedef std::pair<boost::system::error_code, list_iterator> get_result;

    mocked_pool_impl(std::size_t, time_traits::duration) {}

    MOCK_CONST_METHOD0(capacity, std::size_t ());
    MOCK_CONST_METHOD0(size, std::size_t ());
    MOCK_CONST_METHOD0(available, std::size_t ());
    MOCK_CONST_METHOD0(used, std::size_t ());
    MOCK_CONST_METHOD1(get, get_result (time_traits::duration));
    MOCK_CONST_METHOD1(recycle, void (list_iterator));
    MOCK_CONST_METHOD1(waste, void (list_iterator));
    MOCK_CONST_METHOD0(disable, void ());
};

typedef pool<resource, mocked_pool_impl> resource_pool;
typedef resource_pool::handle_ptr resource_handle_ptr;

typedef boost::shared_ptr<resource> resource_ptr;
const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

struct sync_resource_pool : Test {
    mocked_pool_impl::list resources;
    mocked_pool_impl::list_iterator resource_iterator;

    sync_resource_pool()
        : resources(1), resource_iterator(resources.begin()) {}
};

TEST_F(sync_resource_pool, call_capacity_should_call_impl_capacity) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), capacity()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.capacity();
}

TEST_F(sync_resource_pool, call_size_should_call_impl_size) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), size()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.size();
}

TEST_F(sync_resource_pool, call_available_should_call_impl_available) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), available()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.available();
}

TEST_F(sync_resource_pool, call_used_should_call_impl_used) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), used()).WillOnce(Return(0));
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    pool.used();
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_should_call_recycle) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_recycle();

    EXPECT_TRUE(handle);
    EXPECT_EQ(handle->error(), boost::system::error_code());
    EXPECT_FALSE(handle->unusable());
    EXPECT_TRUE(handle->empty());
    EXPECT_NO_THROW(handle->reset(make_resource()));
    EXPECT_NO_THROW(handle->get());
}

TEST_F(sync_resource_pool, get_auto_waste_handle_should_call_waste) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_waste();

    EXPECT_TRUE(handle);
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_recycle();

    handle->recycle();

    EXPECT_TRUE(handle->unusable());
    EXPECT_THROW(handle->recycle(), error::unusable_handle);
    EXPECT_TRUE(handle->empty());
    EXPECT_THROW(handle->get(), error::empty_handle);
}

TEST_F(sync_resource_pool, get_auto_recylce_handle_and_waste_should_call_waste_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_recycle();

    handle->waste();
}

TEST_F(sync_resource_pool, get_auto_waste_handle_and_recycle_should_call_recycle_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), recycle(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_waste();

    handle->recycle();
}

TEST_F(sync_resource_pool, get_auto_waste_handle_and_waste_should_call_waste_once) {
    resource_pool pool(1);

    InSequence s;

    EXPECT_CALL(pool.impl(), get(_)).WillOnce(Return(mocked_pool_impl::get_result(boost::system::error_code(), resource_iterator)));
    EXPECT_CALL(pool.impl(), waste(_)).WillOnce(Return());
    EXPECT_CALL(pool.impl(), disable()).WillOnce(Return());

    const resource_handle_ptr handle = pool.get_auto_waste();

    handle->waste();
}

}
