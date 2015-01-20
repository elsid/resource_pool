#ifndef YAMAIL_RESOURCE_POOL_ERROR_HPP
#define YAMAIL_RESOURCE_POOL_ERROR_HPP

#include <boost/system/error_code.hpp>

namespace yamail {
namespace resource_pool {
namespace error {

struct empty_handle : std::logic_error {
    empty_handle() : std::logic_error("handle is empty") {}
};

struct unusable_handle : std::logic_error {
    unusable_handle() : std::logic_error("handle is unusable") {}
};

enum code {
    get_resource_timeout = 1,
    request_queue_overflow,
    request_queue_is_empty,
    exception,
    disabled
};

namespace detail {

class category : public boost::system::error_category {
public:
    const char* name() const throw() { return "resource_pool"; }

    std::string message(int value) const {
        switch (value) {
            case get_resource_timeout:
                return "get resource timeout";
            case request_queue_overflow:
                return "request queue overflow";
            case request_queue_is_empty:
                return "request queue is empty";
            case exception:
                return "exception";
            case disabled:
                return "resource pool is disabled";
            default:
                return "resource_pool error";
        }
    }
};

}

inline const boost::system::error_category& get_category() {
    static detail::category instance;
    return instance;
}

}
}
}

namespace boost {
namespace system {

template <>
struct is_error_code_enum<yamail::resource_pool::error::code> {
    static const bool value = true;
};

}
}

namespace yamail {
namespace resource_pool {

inline boost::system::error_code make_error_code(const error::code e) {
    return boost::system::error_code(static_cast<int>(e), error::get_category());
}

}
}

#endif
