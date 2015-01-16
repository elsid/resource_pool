#include "tests.hpp"

namespace pool_tests {

using namespace tests;
using namespace yamail::resource_pool::async;

typedef pool<resource> resource_pool;
typedef boost::shared_ptr<resource_pool::handle> resource_handle_ptr;

class callback : public base_callback {
public:
    callback(boost::promise<void>& called) : base_callback(called) {}

    virtual void operator ()(const boost::system::error_code& /*ec*/, resource_handle_ptr /*handle*/) const {
        _impl->call();
        _called.set_value();
    }
};

class reset_resource_if_need : public callback {
public:
    typedef boost::function<resource_ptr ()> make_resource;

    reset_resource_if_need(make_resource res, boost::promise<void>& called)
            : callback(called), _make_resource(res) {}

    virtual void operator ()(const boost::system::error_code& ec, resource_handle_ptr handle) const {
        EXPECT_EQ(ec, boost::system::error_code());
        if (handle->empty()) {
            handle->reset(_make_resource());
        }
        callback::operator ()(ec, handle);
    }

private:
    make_resource _make_resource;
};

class use_handle : public callback {
public:
    typedef void ((use_handle::*strategy)(resource_handle_ptr) const);

    use_handle(strategy use_strategy, boost::promise<void>& called)
            : callback(called), _use_strategy(use_strategy) {}

    virtual void operator ()(const boost::system::error_code& ec, resource_handle_ptr handle) const {
        EXPECT_EQ(ec, boost::system::error_code());
        EXPECT_FALSE(handle->empty());
        if (!handle->empty()) {
            use(handle);
        }
        EXPECT_TRUE(handle->empty());
        EXPECT_THROW(handle->get(), error::empty_handle);
        callback::operator ()(ec, handle);
    }

    void use(resource_handle_ptr handle) const { (this->*_use_strategy)(handle); }
    void recycle(resource_handle_ptr handle) const {handle->recycle(); }
    void waste(resource_handle_ptr handle) const { handle->waste(); }

private:
    strategy _use_strategy;
};

class check_error : public callback {
public:
    check_error(const boost::system::error_code& error, boost::promise<void>& called)
            : callback(called), _error(error) {}

    virtual void operator ()(const boost::system::error_code& ec, resource_handle_ptr handle) const {
        EXPECT_EQ(ec, _error);
        EXPECT_EQ(handle->empty(), _error != boost::system::error_code());
        callback::operator ()(ec, handle);
    }

private:
    boost::system::error_code _error;
};

class my_resource_handle : public handle_facade<resource_pool> {
public:
    my_resource_handle(const resource_handle_ptr& handle)
            : handle_facade<resource_pool>(handle) {}
};

class create_my_resource_handle {
public:
    create_my_resource_handle(boost::promise<void>& called) : _callback(called) {}

    void operator ()(const boost::system::error_code& ec, my_resource_handle handle) const {
        _callback(ec, handle.handle());
    }

private:
    callback _callback;
};

}
