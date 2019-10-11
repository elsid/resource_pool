#ifndef YAMAIL_RESOURCE_POOL_ERROR_HPP
#define YAMAIL_RESOURCE_POOL_ERROR_HPP

#include <boost/system/error_code.hpp>

#include <sstream>

namespace yamail {
namespace resource_pool {
namespace error {

struct empty_handle final : std::logic_error {
    empty_handle() : std::logic_error("handle is empty") {}
};

struct unusable_handle final : std::logic_error {
    unusable_handle() : std::logic_error("handle is unusable") {}
};

struct zero_pool_capacity final : std::logic_error {
    zero_pool_capacity() : std::logic_error("pool capacity is 0") {}
};

enum code {
    ok,
    get_resource_timeout,
    request_queue_overflow,
    disabled,
};

namespace detail {

class category final : public boost::system::error_category {
public:
    const char* name() const noexcept final {
        return "yamail::resource_pool::error::detail::category";
    }

    std::string message(int value) const final {
        switch (code(value)) {
            case ok:
                return "no error";
            case get_resource_timeout:
                return "get resource timeout";
            case request_queue_overflow:
                return "request queue overflow";
            case disabled:
                return "resource pool is disabled";
        }
        std::ostringstream error;
        error << "no message for yamail::resource_pool::error: " << value;
        throw std::logic_error(error.str());
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
namespace error {

inline boost::system::error_code make_error_code(const code e) {
    return boost::system::error_code(static_cast<int>(e), error::get_category());
}

}
}
}

#endif
