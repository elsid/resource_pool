#ifndef YAMAIL_RESOURCE_POOL_OBJECT_FACTORY_HPP
#define YAMAIL_RESOURCE_POOL_OBJECT_FACTORY_HPP

namespace yamail {
namespace resource_pool {

template <class T>
struct object_factory {
    T operator()() { return T(); }
};

}}

#endif