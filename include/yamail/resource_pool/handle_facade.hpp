#ifndef YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP

#include <boost/shared_ptr.hpp>

namespace yamail {
namespace resource_pool {

template <class ResourcePool>
class handle_facade {
public:
    typedef ResourcePool pool;
    typedef typename pool::handle::value_type value_type;
    typedef typename pool::handle::pointer pointer;
    typedef boost::shared_ptr<typename pool::handle> handle_ptr;

    handle_facade(const handle_ptr& handle) : _handle(handle) {}
    virtual ~handle_facade() {}

    handle_ptr handle() const { return _handle; }
    bool unusable() const { return _handle->unusable(); }
    bool empty() const { return _handle->empty(); }
    value_type& get() { return _handle->get(); }
    const value_type& get() const { return _handle->get(); }
    value_type *operator ->() { return &get(); }
    const value_type *operator ->() const { return &get(); }
    value_type &operator *() { return get(); }
    const value_type &operator *() const { return get(); }

    void recycle();
    void waste();
    void reset(pointer res) { _handle->reset(res); }

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

}
}

#endif
