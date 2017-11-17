#include "tests.hpp"

#include <yamail/resource_pool/async/detail/pool_impl.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

struct mocked_queue {
    typedef std::list<detail::idle<resource>>::iterator list_iterator;
    typedef std::function<void (const boost::system::error_code&, list_iterator)> value_type;
    typedef std::function<void ()> callback;

    MOCK_CONST_METHOD4(push, bool (mocked_io_service&, const value_type&, const callback&, time_traits::duration));
    MOCK_CONST_METHOD2(pop, bool (mocked_io_service*&, value_type&));
    MOCK_CONST_METHOD0(size, std::size_t ());

    mocked_queue(std::size_t) {}
};

typedef pool_impl<resource, std::mutex, mocked_io_service, mocked_queue> resource_pool_impl;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;

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

typedef std::shared_ptr<mocked_callback> mocked_callback_ptr;

struct async_resource_pool_impl : Test {
    mocked_io_service ios;

    std::function<void ()> on_get;
    std::function<void ()> on_first_get;
    std::function<void ()> on_second_get;

    mocked_queue::value_type on_get_res;

    std::function<void ()> on_expired;
};

TEST_F(async_resource_pool_impl, create_with_zero_capacity_should_throw_exception) {
    EXPECT_THROW(resource_pool_impl(0, 0, time_traits::duration::max()),
                 error::zero_pool_capacity);
}

TEST_F(async_resource_pool_impl, create_const_with_non_zero_capacity_then_check) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    EXPECT_EQ(pool.capacity(), 1);
}

TEST_F(async_resource_pool_impl, create_const_then_check_size_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    EXPECT_EQ(pool.size(), 0);
}

TEST_F(async_resource_pool_impl, create_const_then_check_available_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    EXPECT_EQ(pool.available(), 0);
}

TEST_F(async_resource_pool_impl, create_const_then_check_used_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    EXPECT_EQ(pool.used(), 0);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_capacity_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max());

    EXPECT_EQ(pool.capacity(), 2);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_size_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max());

    EXPECT_EQ(pool.size(), 2);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_available_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max());

    EXPECT_EQ(pool.available(), 2);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_capacity_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max());

    EXPECT_EQ(pool.capacity(), 2);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_size_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max());

    EXPECT_EQ(pool.size(), 2);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_available_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max());

    EXPECT_EQ(pool.available(), 2);
}


TEST_F(async_resource_pool_impl, create_const_then_check_stats_should_be_0_0_0_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    const async::stats expected {0, 0, 0, 0};

    EXPECT_CALL(pool.queue(), size()).WillOnce(Return(0));

    const auto actual = pool.stats();

    EXPECT_EQ(actual.size, expected.size);
    EXPECT_EQ(actual.available, expected.available);
    EXPECT_EQ(actual.used, expected.used);
    EXPECT_EQ(actual.queue_size, expected.queue_size);
}

TEST_F(async_resource_pool_impl, create_const_then_call_queue_should_succeed) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max());
    EXPECT_NO_THROW(pool.queue());
}

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
    recycle_resource(resource_pool_impl& pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_NE(res, resource_ptr_list_iterator());
        pool.recycle(res);
    }

private:
    resource_pool_impl& pool;
};

class waste_resource {
public:
    waste_resource(resource_pool_impl& pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_NE(res, resource_ptr_list_iterator());
        pool.waste(res);
    }

private:
    resource_pool_impl& pool;
};

TEST_F(async_resource_pool_impl, get_one_should_call_callback) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    mocked_callback_ptr get = std::make_shared<mocked_callback>();

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(*get, call(_, _)).WillOnce(Return());

    pool.get(ios, callback(get));
    on_get();
}

TEST_F(async_resource_pool_impl, get_one_and_recycle_should_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));

    pool.get(ios, recycle_resource(pool));
    on_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_one_and_waste_should_make_no_available_resources) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));

    pool.get(ios, waste_resource(pool));
    on_get();

    EXPECT_EQ(pool.available(), 0);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    pool.get(ios, recycle_resource(pool));
    on_first_get();

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    pool.get(ios, recycle_resource(pool), time_traits::duration(1));
    on_second_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(DoAll(SaveArg<1>(&on_get_res), Return(true)));
    pool.get(ios, recycle_resource(pool));
    pool.get(ios, recycle_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(DoAll(SetArgReferee<0>(&ios), SetArgReferee<1>(on_get_res), Return(true)));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_with_zero_idle_timeout_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration(0));

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(DoAll(SaveArg<1>(&on_get_res), Return(true)));
    pool.get(ios, recycle_resource(pool));
    pool.get(ios, recycle_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(DoAll(SetArgReferee<0>(&ios), SetArgReferee<1>(on_get_res), Return(true)));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_twice_and_waste_then_get_should_use_queue) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(DoAll(SaveArg<1>(&on_get_res), Return(true)));
    pool.get(ios, waste_resource(pool));
    pool.get(ios, waste_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(DoAll(SetArgReferee<0>(&ios), SetArgReferee<1>(on_get_res), Return(true)));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 0);
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

TEST_F(async_resource_pool_impl, get_with_queue_zero_capacity_use_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(Return(false));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));

    pool.get(ios, recycle_resource(pool));
    pool.get(ios, check_error(error::request_queue_overflow), time_traits::duration(1));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_with_queue_use_and_timer_timeout_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(DoAll(SaveArg<2>(&on_expired), Return(true)));

    pool.get(ios, check_no_error());
    pool.get(ios, check_error(error::get_resource_timeout), time_traits::duration(1));
    on_first_get();
    on_expired();
}

TEST_F(async_resource_pool_impl, get_with_queue_use_with_zero_wait_duration_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));

    pool.get(ios, recycle_resource(pool));
    pool.get(ios, check_error(error::get_resource_timeout), time_traits::duration(0));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1);
}

TEST_F(async_resource_pool_impl, get_after_disable_returns_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_get));

    pool.disable();
    pool.get(ios, check_error(error::disabled));
    on_get();
}

TEST_F(async_resource_pool_impl, get_recycled_after_disable_returns_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _, _)).WillOnce(DoAll(SaveArg<1>(&on_get_res), Return(true)));
    pool.get(ios, recycle_resource(pool));
    pool.get(ios, check_error(error::disabled), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(DoAll(SetArgReferee<0>(&ios), SetArgReferee<1>(on_get_res), Return(true)));
    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    pool.disable();
    on_first_get();
    on_second_get();
}

struct custom_exception : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct throw_exception {
    void operator ()(const error_code&, resource_ptr_list_iterator) const {
        throw custom_exception("custom_exception");
    }
};

TEST_F(async_resource_pool_impl, get_and_throw_exception_on_handle_should_pass_exception) {
    resource_pool_impl pool(1, 0, time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    pool.get(ios, throw_exception());
    EXPECT_THROW(on_first_get(), custom_exception);
}

class set_and_recycle_resource {
public:
    set_and_recycle_resource(resource_pool_impl& pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        ASSERT_NE(res, resource_ptr_list_iterator());
        res->value = resource {};
        pool.recycle(res);
    }

private:
    resource_pool_impl& pool;
};

struct check_is_empty {
    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res->value);
    }
};

TEST_F(async_resource_pool_impl, get_one_set_and_recycle_with_zero_idle_timeout_then_get_should_return_empty) {
    resource_pool_impl pool(1, 0, time_traits::duration(0));

    InSequence s;

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop(_, _)).WillOnce(Return(false));
    pool.get(ios, set_and_recycle_resource(pool));
    on_first_get();

    EXPECT_CALL(ios, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    pool.get(ios, check_is_empty(), time_traits::duration(1));
    on_second_get();
}

}
