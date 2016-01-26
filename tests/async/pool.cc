#include <yamail/resource_pool/async/pool.hpp>

#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async;

typedef pool<resource, mocked_io_service, mocked_timer> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;

using boost::system::error_code;

struct async_resource_pool : Test {
    mocked_io_service ios;
    boost::shared_ptr<mocked_timer> timer;

    async_resource_pool() : timer(new mocked_timer()) {}
};

struct mocked_callback {
    MOCK_METHOD2(call, void (const error_code&, resource_handle_ptr));
};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

typedef boost::shared_ptr<mocked_callback> mocked_callback_ptr;

class callback {
public:
    typedef void result_type;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    result_type operator ()(const error_code& err, resource_handle_ptr handle) {
        impl->call(err, handle);
    }

private:
    mocked_callback_ptr impl;
};

class check_error {
public:
    check_error(const error_code& error) : error(error) {}

    void operator ()(const error_code& err, resource_handle_ptr res) const {
        EXPECT_EQ(err, error);
        EXPECT_EQ(res->unusable(), err != error_code());
    }

private:
    const error_code error;
};

TEST_F(async_resource_pool, get_auto_recylce_handle_should_make_one_available_resource) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_recycle(get);

    error_code none;
    check_error check_no_error(none);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_no_error));
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool, get_auto_waste_handle_make_no_available_resources) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_waste(get);

    error_code none;
    check_error check_no_error(none);
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(check_no_error));
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

class recycle_resource {
public:
    void operator ()(const error_code& err, resource_handle_ptr res) {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res->unusable());
        res->recycle();
        handle = res;
    }

private:
    resource_handle_ptr handle;
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_recycle_should_make_one_available_resource) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_recycle(get);

    recycle_resource recycle;
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_recycle_should_make_one_available_resource) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_waste(get);

    recycle_resource recycle;
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(recycle));
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

class waste_resource {
public:
    void operator ()(const error_code& err, resource_handle_ptr res) {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res->unusable());
        res->waste();
        handle = res;
    }

private:
    resource_handle_ptr handle;
};

TEST_F(async_resource_pool, get_auto_recylce_handle_and_waste_should_make_no_available_resources) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_recycle(get);

    waste_resource waste;
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(waste));
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_waste_should_make_no_available_resources) {
    resource_pool pool(ios, timer, 1, 0);

    boost::function<void ()> on_get;
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    mocked_callback_ptr get_impl = make_shared<mocked_callback>();
    callback get(get_impl);
    pool.get_auto_waste(get);

    waste_resource waste;
    EXPECT_CALL(*get_impl, call(_, _)).WillOnce(Invoke(waste));
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

}
