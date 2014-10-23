#ifndef YAMAIL_RESOURCE_POOL_ERROR_HPP
#define YAMAIL_RESOURCE_POOL_ERROR_HPP

#include <stdexcept>

namespace yamail {
namespace resource_pool {
namespace error {

struct add_existing_resource : std::logic_error {
    add_existing_resource()
            : std::logic_error("trying add existing resource") {}
};

struct resource_not_from_pool : std::logic_error {
    resource_not_from_pool()
            : std::logic_error("trying add resource not from current pool") {}
};

struct empty_handle : std::logic_error {
    empty_handle() : std::logic_error("handle is empty") {}
};

struct get_resource_timeout : std::runtime_error {
    get_resource_timeout() : std::runtime_error("get resource timeout") {}
};

}}}

#endif
