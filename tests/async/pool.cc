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
    boost::function<void ()> on_get;

    async_resource_pool() : timer(new mocked_timer()) {}
};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

class check_no_error {
public:
    void operator ()(const error_code& err, resource_handle_ptr res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res->unusable());
    }
};

TEST_F(async_resource_pool, get_auto_recylce_handle_should_make_one_available_resource) {
    resource_pool pool(ios, timer, 1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_recycle(check_no_error());
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool, get_auto_waste_handle_make_no_available_resources) {
    resource_pool pool(ios, timer, 1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_waste(check_no_error());
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

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_recycle(recycle_resource());
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_recycle_should_make_one_available_resource) {
    resource_pool pool(ios, timer, 1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_waste(recycle_resource());
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

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_recycle(waste_resource());
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

TEST_F(async_resource_pool, get_auto_waste_handle_and_waste_should_make_no_available_resources) {
    resource_pool pool(ios, timer, 1, 0);

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.get_auto_waste(waste_resource());
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

}
