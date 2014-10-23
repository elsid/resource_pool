#ifndef YAMAIL_RESOURCE_POOL_ERROR_HPP
#define YAMAIL_RESOURCE_POOL_ERROR_HPP

#include <stdexcept>
#include <map>

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

enum code_value {
    none = 0,
    get_resource_timeout
};

struct code {
    code_value value;

    code(code_value value) : value(value) {}

    operator bool() const { return value != none; }

    bool operator ==(code_value other) const { return value == other; }

    std::string message() const {
        switch (value) {
            case none:
                return "not an error";
            case get_resource_timeout:
                return "get resource timeout";
            default:
                return "unknown error";
        }
    }
};

}}}

#endif
