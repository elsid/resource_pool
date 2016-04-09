#include <yamail/resource_pool/async/detail/pool_impl.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

typedef pool_impl<resource, mocked_io_service, mocked_timer> resource_pool_impl;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;
typedef boost::shared_ptr<resource_pool_impl> resource_pool_impl_ptr;

}

namespace boost {

inline std::ostream& operator <<(std::ostream& stream, resource_ptr_list_iterator res) {
    return stream << res._M_node;
}

}

namespace {

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD2(call, void (const error_code&, resource_ptr_list_iterator));
};

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

struct async_resource_pool_impl : Test {
    mocked_io_service ios;
    boost::shared_ptr<mocked_timer> timer;

    boost::function<void ()> on_get;
    boost::function<void ()> on_first_get;
    boost::function<void ()> on_second_get;

    async_resource_pool_impl() : timer(new mocked_timer()) {}

    resource_pool_impl_ptr make_pool(std::size_t capacity,
                                     std::size_t queue_capacity) {
        return make_shared<resource_pool_impl>(ref(ios), timer,
                                               capacity, queue_capacity);
    }
};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()(const error_code& err,
                            resource_ptr_list_iterator res) const {
        impl->call(err, res);
    }

private:
    mocked_callback_ptr impl;
};

class recycle_resource {
public:
    recycle_resource(resource_pool_impl_ptr pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_NE(res, resource_ptr_list_iterator());
        pool->recycle(res);
    }

private:
    resource_pool_impl_ptr pool;
};

class waste_resource {
public:
    waste_resource(resource_pool_impl_ptr pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_NE(res, resource_ptr_list_iterator());
        pool->waste(res);
    }

private:
    resource_pool_impl_ptr pool;
};

TEST_F(async_resource_pool_impl, get_one_should_call_callback) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    mocked_callback_ptr get = make_shared<mocked_callback>();

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(*get, call(_, _)).WillOnce(Return());

    pool->get(callback(get));
    on_get();
}

TEST_F(async_resource_pool_impl, get_one_and_recycle_should_make_one_available_resource) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool->get(recycle_resource(pool));
    on_get();

    EXPECT_EQ(pool->available(), 1);
}

TEST_F(async_resource_pool_impl, get_one_and_waste_should_make_no_available_resources) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool->get(waste_resource(pool));
    on_get();

    EXPECT_EQ(pool->available(), 0);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(Return());
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));

    pool->get(recycle_resource(pool));
    pool->get(recycle_resource(pool), seconds(1));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool->available(), 1);
}

class check_error {
public:
    check_error(const error_code& error) : error(error) {}
    check_error(const error::code& error) : error(make_error_code(error)) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error);
        EXPECT_EQ(res != resource_ptr_list_iterator(), error == error_code());
    }

private:
    const error_code error;
};

struct check_no_error : check_error {
    check_no_error() : check_error(error_code()) {}
};

TEST_F(async_resource_pool_impl, get_with_queue_use_and_timer_timeout_should_return_error) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    boost::function<void (error_code)> on_async_wait;
    time_point expire_time;

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));

    pool->get(check_no_error());
    on_first_get();
    pool->get(check_error(error::get_resource_timeout), seconds(1));

    EXPECT_CALL(*timer, expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));

    on_async_wait(error_code());
    on_second_get();
}

TEST_F(async_resource_pool_impl, get_with_queue_use_with_zero_wait_duration_should_return_error) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(*timer, expires_at(_)).Times(0);
    EXPECT_CALL(*timer, async_wait(_)).Times(0);
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));

    pool->get(recycle_resource(pool));
    pool->get(check_error(error::get_resource_timeout), seconds(0));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool->available(), 1);
}

TEST_F(async_resource_pool_impl, get_after_disable_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool->disable();
    pool->get(check_error(error::disabled));
    on_get();
}

TEST_F(async_resource_pool_impl, get_recycled_after_disable_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(Return());
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));

    pool->get(recycle_resource(pool));
    pool->get(check_error(error::disabled), seconds(1));
    pool->disable();
    on_first_get();
    on_second_get();
}

}
