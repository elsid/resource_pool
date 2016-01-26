#include <yamail/resource_pool/async/detail/pool_impl.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

typedef pool_impl<resource, mocked_io_service, mocked_timer> resource_pool_impl;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;
typedef resource_pool_impl::list_iterator_opt resource_ptr_list_iterator_opt;
typedef boost::shared_ptr<resource_pool_impl> resource_pool_impl_ptr;

using boost::system::error_code;

struct async_resource_pool_impl : Test {
    mocked_io_service ios;
    boost::shared_ptr<mocked_timer> timer;

    async_resource_pool_impl() : timer(new mocked_timer()) {}

    resource_pool_impl_ptr make_pool(std::size_t capacity,
                                     std::size_t queue_capacity) {
        return make_shared<resource_pool_impl>(ref(ios), timer,
                                               capacity, queue_capacity);
    }
};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

struct mocked_callback {
    MOCK_CONST_METHOD2(call, void (const error_code&, const resource_ptr_list_iterator_opt&));
};

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()(const error_code& err,
                            const resource_ptr_list_iterator_opt& res) const {
        impl->call(err, res);
    }

private:
    mocked_callback_ptr impl;
};

class recycle_resource {
public:
    recycle_resource(resource_pool_impl_ptr pool) : pool(pool) {}

    void operator ()(const error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_TRUE(res.is_initialized());
        pool->recycle(*res);
    }

private:
    resource_pool_impl_ptr pool;
};

class waste_resource {
public:
    waste_resource(resource_pool_impl_ptr pool) : pool(pool) {}

    void operator ()(const error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_TRUE(res.is_initialized());
        pool->waste(*res);
    }

private:
    resource_pool_impl_ptr pool;
};

TEST_F(async_resource_pool_impl, get_one_and_recycle_should_make_one_available_resource) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    recycle_resource recycle(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_get();

    EXPECT_EQ(pool->available(), 1);
}

TEST_F(async_resource_pool_impl, get_one_and_waste_should_make_no_available_resources) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    waste_resource waste(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(waste));
    on_get();

    EXPECT_EQ(pool->available(), 0);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    boost::function<void ()> on_first_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    EXPECT_CALL(*timer, expires_at(_)).WillOnce(Return());
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(Return());
    pool->get(get, seconds(1));

    recycle_resource recycle(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    boost::function<void ()> on_second_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    on_first_get();

    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_second_get();

    EXPECT_EQ(pool->available(), 1);
}

class check_error {
public:
    check_error(const error_code& error) : error(error) {}

    void operator ()(const error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, error);
        EXPECT_EQ(res.is_initialized(), error == error_code());
    }

private:
    const error_code error;
};

TEST_F(async_resource_pool_impl, get_with_queeu_use_and_timer_timeout_should_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    boost::function<void ()> on_first_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    error_code none;
    check_error check_no_error(none);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_no_error));
    on_first_get();

    time_point expire_time;
    EXPECT_CALL(*timer, expires_at(_)).WillOnce(SaveArg<0>(&expire_time));
    boost::function<void (error_code)> on_async_wait;
    EXPECT_CALL(*timer, async_wait(_)).WillOnce(SaveArg<0>(&on_async_wait));
    pool->get(get, seconds(1));

    boost::function<void ()> on_second_get;
    EXPECT_CALL(*timer, expires_at()).WillOnce(Return(expire_time));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    on_async_wait(error_code());

    check_error check_get_resource_timeout(make_error_code(error::get_resource_timeout));
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_get_resource_timeout));
    on_second_get();
}

TEST_F(async_resource_pool_impl, get_with_queue_use_with_zero_wait_duration_should_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 1);

    boost::function<void ()> on_first_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    EXPECT_CALL(*timer, expires_at(_)).Times(0);
    EXPECT_CALL(*timer, async_wait(_)).Times(0);
    boost::function<void ()> on_second_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    pool->get(get);

    recycle_resource recycle(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_first_get();

    check_error check_get_resource_timeout(make_error_code(error::get_resource_timeout));
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_get_resource_timeout));
    on_second_get();

    EXPECT_EQ(pool->available(), 1);
}

TEST_F(async_resource_pool_impl, get_after_disable_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    pool->disable();

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    check_error check_disabled(make_error_code(error::disabled));
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_disabled));
    on_get();
}

TEST_F(async_resource_pool_impl, get_recycled_after_disable_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    boost::function<void ()> on_first_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    pool->disable();

    boost::function<void ()> on_second_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    pool->get(get);

    recycle_resource recycle(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_first_get();

    check_error check_disabled(make_error_code(error::disabled));
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_disabled));
    on_second_get();
}

TEST_F(async_resource_pool_impl, get_new_after_disable_returns_error) {
    resource_pool_impl_ptr pool = make_pool(1, 0);

    boost::function<void ()> on_first_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool->get(get);

    pool->disable();

    boost::function<void ()> on_second_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    pool->get(get);

    waste_resource waste(pool);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(waste));
    on_first_get();

    check_error check_disabled(make_error_code(error::disabled));
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_disabled));
    on_second_get();
}

}
