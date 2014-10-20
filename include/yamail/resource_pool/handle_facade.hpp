#ifndef YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP
#define YAMAIL_RESOURCE_POOL_HANDLE_FACADE_HPP

namespace yamail {
namespace resource_pool {

template <class ResourcePool>
class handle_facade {
public:
    typedef ResourcePool resource_pool;
    typedef typename resource_pool::resource resource;
    typedef typename resource_pool::handle_ptr handle_ptr;

    handle_facade(const handle_ptr& handle) : _handle(handle) {}
    virtual ~handle_facade() {}

    bool empty() const { return _handle->empty(); }
    resource& get() { return _handle->get(); }
    const resource& get() const { return _handle->get(); }
    resource *operator ->() { return &get(); }
    const resource *operator ->() const { return &get(); }
    resource &operator *() { return get(); }
    const resource &operator *() const { return get(); }

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
