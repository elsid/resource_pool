#include "tests.hpp"

#include <yamail/resource_pool/async/detail/pool_impl.hpp>

#include <boost/optional/optional_io.hpp>

namespace {

using namespace tests;
using namespace yamail::resource_pool;
using namespace yamail::resource_pool::async::detail;

struct mocked_queue {
    using list_iterator = std::list<detail::idle<resource>>::iterator;
    using value_type = list_iterator_handler<resource>;
    using queued_value_t = queued_value<value_type, mocked_io_context>;

    MOCK_CONST_METHOD3(push, bool (mocked_io_context&, time_traits::duration, const value_type&));
    MOCK_CONST_METHOD0(pop, boost::optional<queued_value_t> ());
    MOCK_CONST_METHOD0(size, std::size_t ());

    mocked_queue(std::size_t) {}
};

using resource_pool_impl = pool_impl<resource, std::mutex, mocked_io_context, mocked_queue>;
using resource_ptr_list_iterator = resource_pool_impl::list_iterator;
using queued_value_t = mocked_queue::queued_value_t;

auto make_queued_value(mocked_queue::value_type&& request, mocked_io_context& io) {
    return boost::optional<queued_value_t>(queued_value_t {std::move(request), io});
}

}

namespace yamail::resource_pool::async::detail {

template <class V, class I>
inline std::ostream& operator <<(std::ostream& stream, const queued_value<V, I>&) {
    return stream;
}

}

namespace boost {

inline std::ostream& operator <<(std::ostream& stream, resource_ptr_list_iterator res) {
    return stream << &*res;
}

}

namespace {

using boost::system::error_code;

struct mocked_callback {
    MOCK_CONST_METHOD2(call, void (const error_code&, resource_ptr_list_iterator));
};

using mocked_callback_ptr = std::shared_ptr<mocked_callback>;

struct async_resource_pool_impl : Test {
    StrictMock<executor_gmock> executor;
    mocked_executor executor_wrapper {&executor};
    mocked_io_context io {&executor_wrapper};

    std::function<void ()> on_get;
    std::function<void ()> on_first_get;
    std::function<void ()> on_second_get;

    mocked_queue::value_type on_get_res;
};

TEST_F(async_resource_pool_impl, create_with_zero_capacity_should_throw_exception) {
    EXPECT_THROW(resource_pool_impl(0, 0, time_traits::duration::max(), time_traits::duration::max()),
                 error::zero_pool_capacity);
}

TEST_F(async_resource_pool_impl, create_const_with_non_zero_capacity_then_check) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    EXPECT_EQ(pool.capacity(), 1u);
}

TEST_F(async_resource_pool_impl, create_const_then_check_size_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    EXPECT_EQ(pool.size(), 0u);
}

TEST_F(async_resource_pool_impl, create_const_then_check_available_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    EXPECT_EQ(pool.available(), 0u);
}

TEST_F(async_resource_pool_impl, create_const_then_check_used_should_be_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    EXPECT_EQ(pool.used(), 0u);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_capacity_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.capacity(), 2u);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_size_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.size(), 2u);
}

TEST_F(async_resource_pool_impl, create_const_with_range_len2_then_check_available_should_be_2) {
    std::vector<resource> res;
    res.emplace_back();
    res.emplace_back();
    const resource_pool_impl pool(
        std::make_move_iterator(std::begin(res)), std::make_move_iterator(std::end(res)),
        0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.available(), 2u);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_capacity_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.capacity(), 2u);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_size_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.size(), 2u);
}

TEST_F(async_resource_pool_impl, create_const_with_generator_and_capacity_2_then_check_available_should_be_2) {
    const resource_pool_impl pool([]{ return resource{}; }, 2, 0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_EQ(pool.available(), 2u);
}


TEST_F(async_resource_pool_impl, create_const_then_check_stats_should_be_0_0_0_0) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    const async::stats expected {0, 0, 0, 0};

    EXPECT_CALL(pool.queue(), size()).WillOnce(Return(0));

    const auto actual = pool.stats();

    EXPECT_EQ(actual.size, expected.size);
    EXPECT_EQ(actual.available, expected.available);
    EXPECT_EQ(actual.used, expected.used);
    EXPECT_EQ(actual.queue_size, expected.queue_size);
}

TEST_F(async_resource_pool_impl, create_const_then_call_queue_should_succeed) {
    const resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());
    EXPECT_NO_THROW(pool.queue());
}

class callback {
public:
    using result_type = void;

    callback() = default;

    callback(const callback&) = delete;

    callback(callback&&) = default;

    callback(const mocked_callback_ptr& impl) : impl(impl) {}

    callback& operator =(const callback&) = delete;

    callback& operator =(callback&&) = default;

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
        pool.waste(res);
    }

private:
    resource_pool_impl& pool;
};

ACTION_P(SaveMoveArg2, ptr) {
    *ptr = std::move(const_cast<std::decay_t<decltype(arg2)>&>(arg2));
}

TEST_F(async_resource_pool_impl, get_one_should_call_callback) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    mocked_callback_ptr get = std::make_shared<mocked_callback>();

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(*get, call(_, _)).WillOnce(Return());

    pool.get(io, callback(get));
    on_get();
}

TEST_F(async_resource_pool_impl, get_one_and_recycle_should_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));

    pool.get(io, recycle_resource(pool));
    on_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_one_and_waste_should_make_no_available_resources) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));

    pool.get(io, waste_resource(pool));
    on_get();

    EXPECT_EQ(pool.available(), 0u);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, recycle_resource(pool));
    on_first_get();

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, recycle_resource(pool), time_traits::duration(1));
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, recycle_resource(pool));
    pool.get(io, recycle_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_twice_and_recycle_with_zero_idle_timeout_should_use_queue_and_make_one_available_resource) {
    resource_pool_impl pool(1, 0, time_traits::duration(0), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, recycle_resource(pool));
    pool.get(io, recycle_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_twice_and_waste_then_get_should_use_queue) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, waste_resource(pool));
    pool.get(io, waste_resource(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 0u);
}

class check_error {
public:
    check_error(const error_code& error) : error(error) {}
    check_error(const error::code& error) : error(make_error_code(error)) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator) const {
        EXPECT_EQ(err, error);
    }

private:
    const error_code error;
};

struct check_no_error : check_error {
    check_no_error() : check_error(error_code()) {}
};

TEST_F(async_resource_pool_impl, get_with_queue_zero_capacity_use_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(Return(false));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));

    pool.get(io, recycle_resource(pool));
    pool.get(io, check_error(error::request_queue_overflow), time_traits::duration(1));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_with_queue_use_and_timer_timeout_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));

    pool.get(io, check_no_error());
    pool.get(io, check_error(error::get_resource_timeout), time_traits::duration(1));
    on_first_get();
    on_get_res(make_error_code(error::get_resource_timeout));
}

TEST_F(async_resource_pool_impl, get_with_queue_use_with_zero_wait_duration_should_return_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));

    pool.get(io, recycle_resource(pool));
    pool.get(io, check_error(error::get_resource_timeout), time_traits::duration(0));
    on_first_get();
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, get_after_disable_returns_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    EXPECT_CALL(executor, dispatch(_)).WillOnce(SaveArg<0>(&on_get));

    pool.disable();
    pool.get(io, check_error(error::disabled));
    on_get();
}

TEST_F(async_resource_pool_impl, get_recycled_after_disable_returns_error) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, recycle_resource(pool));
    pool.get(io, check_error(error::disabled), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, dispatch(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.disable();
    on_first_get();
    on_second_get();
}

class set_and_recycle_resource {
public:
    set_and_recycle_resource(resource_pool_impl& pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        res->value = resource {};
        res->reset_time = time_traits::now();
        pool.recycle(res);
    }

private:
    resource_pool_impl& pool;
};

struct assert_empty {
public:
    assert_empty(resource_pool_impl& pool) : pool(pool) {}

    void operator ()(const error_code& err, resource_ptr_list_iterator res) const {
        EXPECT_EQ(err, error_code());
        EXPECT_FALSE(res->value);
        pool.waste(res);
    }

private:
    resource_pool_impl& pool;
};

TEST_F(async_resource_pool_impl, get_one_set_and_recycle_with_zero_idle_timeout_then_get_should_return_empty) {
    resource_pool_impl pool(1, 0, time_traits::duration(0), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, set_and_recycle_resource(pool));
    on_first_get();

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, assert_empty(pool), time_traits::duration(1));
    on_second_get();
}

TEST_F(async_resource_pool_impl, should_waste_resource_when_lifespan_ends) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration(0));

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, set_and_recycle_resource(pool));
    on_first_get();

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, assert_empty(pool), time_traits::duration(1));
    on_second_get();
}

TEST_F(async_resource_pool_impl, should_waste_resource_when_lifespan_ends_and_queue_is_not_empty) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration(0));

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, set_and_recycle_resource(pool));
    pool.get(io, assert_empty(pool), time_traits::duration(1));

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    on_first_get();
    on_second_get();
}

TEST_F(async_resource_pool_impl, should_waste_used_resource_after_invalidate) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, set_and_recycle_resource(pool));
    pool.invalidate();
    on_first_get();

    EXPECT_EQ(pool.available(), 0u);

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, assert_empty(pool), time_traits::duration(1));
    on_second_get();
}

TEST_F(async_resource_pool_impl, should_waste_available_resource_after_invalidate) {
    resource_pool_impl pool([]{ return resource{}; }, 1, 0, time_traits::duration::max(), time_traits::duration::max());

    pool.invalidate();

    EXPECT_EQ(pool.available(), 0u);
}

TEST_F(async_resource_pool_impl, should_restore_wasted_cell) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, set_and_recycle_resource(pool));
    pool.invalidate();
    on_first_get();

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    pool.get(io, set_and_recycle_resource(pool), time_traits::duration(1));
    on_second_get();

    EXPECT_EQ(pool.available(), 1u);
}

TEST_F(async_resource_pool_impl, should_waste_used_resource_after_invalidate_when_queue_is_not_empty) {
    resource_pool_impl pool(1, 0, time_traits::duration::max(), time_traits::duration::max());

    InSequence s;

    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_first_get));
    EXPECT_CALL(pool.queue(), push(_, _, _)).WillOnce(DoAll(SaveMoveArg2(&on_get_res), Return(true)));
    pool.get(io, set_and_recycle_resource(pool));
    pool.get(io, assert_empty(pool), time_traits::duration(1));
    pool.invalidate();

    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(make_queued_value(std::move(on_get_res), io))));
    EXPECT_CALL(executor, post(_)).WillOnce(SaveArg<0>(&on_second_get));
    EXPECT_CALL(pool.queue(), pop()).WillOnce(Return(ByMove(boost::none)));
    on_first_get();
    on_second_get();
}

}
