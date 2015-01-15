#include "tests.hpp"

namespace {

using namespace tests;
using namespace yamail::resource_pool::async::detail;

typedef pool_impl<resource_ptr> resource_pool_impl;
typedef resource_pool_impl::resource_list_iterator resource_list_iterator;
typedef resource_pool_impl::resource_list_iterator_opt resource_list_iterator_opt;
typedef boost::shared_ptr<resource_pool_impl> resource_pool_impl_ptr;
typedef resource_pool_impl::make_resource_callback_succeed make_resource_callback_succeed;
typedef resource_pool_impl::make_resource_callback_failed make_resource_callback_failed;

void make_resource(make_resource_callback_succeed succeed, make_resource_callback_failed failed) {
    try {
        succeed(make_shared<resource>());
        return;
    } catch (const std::exception& e) {
        std::cerr << __func__ << " error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << __func__ << " unknown error" << std::endl;
    }
    failed();
}

struct async_resource_pool_impl_simple : Test {};

TEST(async_resource_pool_impl_simple, dummy_create_not_empty_with_factory) {
    io_service ios;
    resource_pool_impl_ptr pool_impl = make_shared<resource_pool_impl>(ref(ios),
        42, 42, make_resource);
}

struct async_resource_pool_impl_complex : public async_test {
    resource_pool_impl_ptr make_resource_pool_impl(std::size_t capacity,
            std::size_t queue_capacity) {
        return make_shared<resource_pool_impl>(ref(*_io_service), capacity,
            queue_capacity, make_resource);
    }
};

struct callback : base_callback {
    callback(boost::promise<void>& called) : base_callback(called) {}

    void operator ()(const error::code& /*err*/, const resource_list_iterator_opt& /*res*/) const {
        _impl->call();
        _called.set_value();
    }
};

class use_resource : protected callback {
public:
    typedef void ((use_resource::*strategy)(resource_list_iterator) const);

    use_resource(resource_pool_impl_ptr pool_impl,
            strategy use_strategy, boost::promise<void>& called)
            : callback(called), _pool_impl(pool_impl),
              _use_strategy(use_strategy) {}

    void operator ()(const error::code& err, const resource_list_iterator_opt& res) const {
        EXPECT_EQ(err, error::none);
        EXPECT_TRUE(res);
        if (res) {
            use(*res);
        }
        callback::operator ()(err, res);
    }

    void use(resource_list_iterator res) const { (this->*_use_strategy)(res); }
    void recycle(resource_list_iterator res) const { _pool_impl->recycle(res); }
    void waste(resource_list_iterator res) const { _pool_impl->waste(res); }

private:
    resource_pool_impl_ptr _pool_impl;
    strategy _use_strategy;
};

TEST_F(async_resource_pool_impl_complex, get_one_and_recycle_succeed) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    use_resource recycle_resource(pool_impl, &use_resource::recycle, called);
    pool_impl->get(recycle_resource, seconds(1));
    called.get_future().get();
}

TEST_F(async_resource_pool_impl_complex, get_one_and_waste_succeed) {
    boost::promise<void> called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    use_resource waste_resource(pool_impl, &use_resource::waste, called);
    pool_impl->get(waste_resource, seconds(1));
    called.get_future().get();
}

struct check_get_resource_timeout : callback {
    check_get_resource_timeout(boost::promise<void>& called)
            : callback(called) {}

    void operator ()(const error::code& err, const resource_list_iterator_opt& res) const {
        EXPECT_EQ(err, error::get_resource_timeout);
        EXPECT_FALSE(res);
        callback::operator ()(err, res);
    }
};

TEST_F(async_resource_pool_impl_complex, get_more_than_capacity_returns_error) {
    boost::promise<void> first_called;
    boost::promise<void> second_called;
    resource_pool_impl_ptr pool_impl = make_resource_pool_impl(1, 1);
    callback do_nothing(first_called);
    check_get_resource_timeout use(second_called);
    pool_impl->get(do_nothing);
    first_called.get_future().get();
    pool_impl->get(use);
    second_called.get_future().get();
}

}
