#ifndef YAMAIL_RESOURCE_POOL_ERROR_HPP
#define YAMAIL_RESOURCE_POOL_ERROR_HPP

#include <stdexcept>
#include <map>
#include <sstream>

namespace yamail {
namespace resource_pool {
namespace error {

struct add_existing_resource : std::logic_error {
    add_existing_resource()
            : std::logic_error("trying add existing resource") {}
};

struct empty_handle : std::logic_error {
    empty_handle() : std::logic_error("handle is empty") {}
};

enum code_value {
    none = 0,
    get_resource_timeout,
    request_queue_overflow,
    request_queue_is_empty
};

class code {
public:
    code(code_value value = none) : _value(value) {}

    bool operator ==(code other) const { return _value == other._value; }
    bool operator !=(code other) const { return !operator ==(other); }
    bool operator ==(code_value other) const { return _value == other; }
    bool operator !=(code_value other) const { return !operator ==(other); }

    code_value value() const { return _value; }

    std::string message() const {
        switch (_value) {
            case none:
                return "not an error";
            case get_resource_timeout:
                return "get resource timeout";
            case request_queue_overflow:
                return "request queue overflow";
            case request_queue_is_empty:
                return "request queue is empty";
            default:
                return "unknown error";
        }
    }

private:
    code_value _value;
};

inline std::ostream& operator <<(std::ostream& stream, const code& c) {
    return stream << c.message();
}

}}}

#endif
