#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool::async::detail;

typedef pool_impl<resource> resource_pool_impl;
typedef resource_pool_impl::list_iterator resource_ptr_list_iterator;
typedef resource_pool_impl::list_iterator_opt resource_ptr_list_iterator_opt;
typedef boost::shared_ptr<resource_pool_impl> resource_pool_impl_ptr;

struct async_resource_pool_impl_simple : Test {};

const boost::function<resource_ptr ()> make_resource = make_shared<resource>;

TEST(async_resource_pool_impl_simple, create_not_empty_should_succeed) {
    io_service ios;
    resource_pool_impl_ptr pool_impl = make_shared<resource_pool_impl>(ref(ios),
        42, 42);
}

struct async_resource_pool_impl_complex : public async_test {
    resource_pool_impl_ptr make_resource_pool_impl(std::size_t capacity,
            std::size_t queue_capacity) {
        return make_shared<resource_pool_impl>(ref(*_io_service), capacity,
            queue_capacity);
    }
};

struct callback : base_callback {
    callback(boost::promise<void>& called) : base_callback(called) {}

    void operator ()(const boost::system::error_code& /*err*/, const resource_ptr_list_iterator_opt& /*res*/) const {
        _impl->call();
        _called.set_value();
    }
};

class use_resource : protected callback {
public:
    typedef void ((use_resource::*strategy)(resource_ptr_list_iterator) const);

    use_resource(resource_pool_impl_ptr pool_impl,
            strategy use_strategy, boost::promise<void>& called)
            : callback(called), _pool_impl(pool_impl),
              _use_strategy(use_strategy) {}

    void operator ()(const boost::system::error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, boost::system::error_code());
        EXPECT_TRUE(res);
        if (res) {
            use(*res);
        }
        callback::operator ()(err, res);
    }

    void use(resource_ptr_list_iterator res) const { (this->*_use_strategy)(res); }
    void recycle(resource_ptr_list_iterator res) const { _pool_impl->recycle(res); }
    void waste(resource_ptr_list_iterator res) const { _pool_impl->waste(res); }

protected:
    resource_pool_impl_ptr _pool_impl;

private:
    strategy _use_strategy;
};

class set_and_use_resource : public use_resource {
public:
    typedef boost::function<resource_ptr ()> make_resource;

    set_and_use_resource(resource_pool_impl_ptr pool_impl,
            make_resource make_res, strategy use_strategy,
            boost::promise<void>& called)
            : use_resource(pool_impl, use_strategy, called),
              _make_resource(make_res) {}

    void operator ()(const boost::system::error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, boost::system::error_code());
        EXPECT_TRUE(res);
        **res = _make_resource();
        use_resource::operator ()(err, res);
    }

private:
    const make_resource _make_resource;
};

TEST_F(async_resource_pool_impl_complex, get_one_and_recycle_succeed) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const set_and_use_resource add_and_recycle(pool_impl, make_resource,
        &use_resource::recycle, called);
    pool_impl->get(add_and_recycle, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_impl_complex, get_one_and_waste_succeed) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const set_and_use_resource add_and_waste(pool_impl, make_resource,
        &use_resource::waste, called);
    pool_impl->get(add_and_waste, seconds(1));
    called.get_future().get();
}

class check_error : callback {
public:
    check_error(boost::promise<void>& called,
            const boost::system::error_code& error)
            : callback(called), _error(error) {}

    void operator ()(const boost::system::error_code& err, const resource_ptr_list_iterator_opt& res) const {
        EXPECT_EQ(err, _error);
        EXPECT_EQ(res.is_initialized(), _error == boost::system::error_code());
        callback::operator ()(err, res);
    }

private:
    boost::system::error_code _error;
};

TEST_F(async_resource_pool_impl_complex, get_more_than_capacity_returns_error) {
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const callback do_nothing(first_called);
    const check_error check(second_called, make_error_code(error::get_resource_timeout));
    pool_impl->get(do_nothing);
    first_called.get_future().get();
    pool_impl->get(check);
    second_called.get_future().get();
}

TEST_F(async_resource_pool_impl_complex, get_after_disable_returns_error) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const check_error check(called, make_error_code(error::disabled));
    pool_impl->disable();
    pool_impl->get(check);
    called.get_future().get();
}

struct get_disable_and_use : use_resource {
    get_disable_and_use(resource_pool_impl_ptr pool_impl,
            use_resource::strategy use_strategy, boost::promise<void>& called)
            : use_resource(pool_impl, use_strategy, called) {}

    void operator ()(const boost::system::error_code& err, const resource_ptr_list_iterator_opt& res) const {
        boost::promise<void> called;
        const check_error check(called, make_error_code(error::disabled));
        _pool_impl->get(check, seconds(1));
        _pool_impl->disable();
        use_resource::operator ()(err, res);
        called.get_future().get();
    }
};

TEST_F(async_resource_pool_impl_complex, get_recycled_after_disable_returns_error) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const get_disable_and_use use(pool_impl, &use_resource::recycle, called);
    pool_impl->get(use);
    called.get_future().get();
}

TEST_F(async_resource_pool_impl_complex, get_new_after_disable_returns_error) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    const get_disable_and_use use(pool_impl, &use_resource::waste, called);
    pool_impl->get(use);
    called.get_future().get();
}

}
