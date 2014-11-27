#include "tests.hpp"

namespace pool_tests {

using namespace tests;
using namespace yamail::resource_pool::async;

typedef pool<resource_ptr> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;
typedef resource_pool::make_handle_ptr make_resource_handle_ptr;

inline void make_resource(make_resource_handle_ptr handle) {
    try {
        handle->set(make_shared<resource>());
    } catch (const std::exception& e) {
        std::cerr << __func__ << " error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << __func__ << " unknown error" << std::endl;
    }
}

class callback : public base_callback {
public:
    callback(boost::promise<void>& called) : base_callback(called) {}

    virtual void operator ()(resource_handle_ptr /*handle*/) const {
        _impl->call();
        _called.set_value();
    }
};

class use_handle : public callback {
public:
    typedef void ((use_handle::*strategy)(resource_handle_ptr) const);

    use_handle(strategy use_strategy, boost::promise<void>& called)
            : callback(called), _use_strategy(use_strategy) {}

    virtual void operator ()(resource_handle_ptr handle) const {
        EXPECT_EQ(handle->error(), error::none);
        EXPECT_FALSE(handle->empty());
        if (!handle->empty()) {
            use(handle);
        }
        EXPECT_TRUE(handle->empty());
        EXPECT_THROW(handle->get(), error::empty_handle);
        callback::operator ()(handle);
    }

    void use(resource_handle_ptr handle) const { (this->*_use_strategy)(handle); }
    void recycle(resource_handle_ptr handle) const {handle->recycle(); }
    void waste(resource_handle_ptr handle) const { handle->waste(); }

private:
    strategy _use_strategy;
};

class check_error : public callback {
public:
    check_error(const error::code& error, boost::promise<void>& called)
            : callback(called), _error(error) {}

    virtual void operator ()(resource_handle_ptr handle) const {
        EXPECT_EQ(handle->error(), _error);
        EXPECT_EQ(handle->empty(), _error != error::none);
        callback::operator ()(handle);
    }

private:
    error::code _error;
};

class my_resource_handle : public handle_facade<resource_pool> {
public:
    my_resource_handle(const resource_handle_ptr& handle)
            : handle_facade<resource_pool>(handle) {}
};

class create_my_resource_handle {
public:
    create_my_resource_handle(boost::promise<void>& called) : _callback(called) {}

    void operator ()(my_resource_handle handle) const {
        _callback(handle.handle());
    }

private:
    callback _callback;
};

}
