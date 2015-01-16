#ifndef YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP

#include <boost/shared_ptr.hpp>

namespace yamail {
namespace resource_pool {

template <class ResourcePool>
class handle_facade {
public:
    typedef ResourcePool resource_pool;
    typedef typename resource_pool::value_type value_type;
    typedef boost::shared_ptr<typename resource_pool::handle> handle_ptr;

    handle_facade(const handle_ptr& handle) : _handle(handle) {}
    virtual ~handle_facade() {}

    handle_ptr handle() const { return _handle; }
    boost::system::error_code error() const { return _handle->error(); }
    bool empty() const { return _handle->empty(); }
    value_type& get() { return _handle->get(); }
    const value_type& get() const { return _handle->get(); }
    value_type *operator ->() { return &get(); }
    const value_type *operator ->() const { return &get(); }
    value_type &operator *() { return get(); }
    const value_type &operator *() const { return get(); }

    void recycle();
    void waste();

protected:
    virtual void before_recycle() {}
    virtual void before_waste() {}

private:
    handle_ptr _handle;
};

template <class P>
void handle_facade<P>::recycle() {
    before_recycle();
    _handle->recycle();
}

template <class P>
void handle_facade<P>::waste() {
    before_waste();
    _handle->waste();
}

}}

#endif
