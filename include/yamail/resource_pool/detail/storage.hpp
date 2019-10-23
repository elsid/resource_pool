#pragma once

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/time_traits.hpp>
#include <yamail/resource_pool/detail/idle.hpp>

#include <algorithm>
#include <list>

namespace yamail {
namespace resource_pool {
namespace detail {

struct storage_stats {
    std::size_t available;
    std::size_t used;
    std::size_t wasted;
};

template <class T>
class storage {
public:
    using cell_iterator = typename std::list<idle<T>>::iterator;

    inline storage(std::size_t capacity, time_traits::duration idle_timeout);

    template <class Generator>
    inline storage(Generator&& generator, std::size_t capacity, time_traits::duration idle_timeout);

    template <class InputIterator>
    inline storage(InputIterator begin, InputIterator end, time_traits::duration idle_timeout);

    storage(const storage& other) = delete;

    storage(storage&& other) = default;

    inline storage_stats stats() const;

    inline boost::optional<cell_iterator> lease();

    inline void recycle(cell_iterator cell);

    inline void waste(cell_iterator cell);

private:
    time_traits::duration idle_timeout_;
    std::list<idle<T>> available_;
    std::list<idle<T>> used_;
    std::list<idle<T>> wasted_;
};

template <class T>
storage<T>::storage(std::size_t capacity, time_traits::duration idle_timeout)
        : idle_timeout_(idle_timeout),
          wasted_(capacity) {
}

template <class T>
template <class Generator>
storage<T>::storage(Generator&& generator, std::size_t capacity, time_traits::duration idle_timeout)
        : idle_timeout_(idle_timeout) {
    const auto drop_time = time_traits::add(time_traits::now(), idle_timeout_);
    for (std::size_t i = 0; i < capacity; ++i) {
        available_.emplace_back(generator(), drop_time);
    }
}

template <class T>
template <class InputIterator>
storage<T>::storage(InputIterator begin, InputIterator end, time_traits::duration idle_timeout)
        : idle_timeout_(idle_timeout) {
    const auto drop_time = time_traits::add(time_traits::now(), idle_timeout_);
    std::for_each(begin, end, [&] (auto&& v) {
        available_.emplace_back(std::forward<decltype(v)>(v), drop_time);
    });
}

template <class T>
storage_stats storage<T>::stats() const {
    storage_stats result;
    result.available = available_.size();
    result.used = used_.size();
    result.wasted = wasted_.size();
    return result;
}

template <class T>
boost::optional<typename storage<T>::cell_iterator> storage<T>::lease() {
    const auto now = time_traits::now();
    while (!available_.empty()) {
        const auto candidate = available_.begin();
        if (candidate->drop_time > now) {
            used_.splice(used_.end(), available_, candidate);
            return candidate;
        }
        candidate->value.reset();
        wasted_.splice(wasted_.end(), available_, candidate);
    }
    if (!wasted_.empty()) {
        const auto result = wasted_.begin();
        used_.splice(used_.end(), wasted_, result);
        return result;
    }
    return {};
}

template <class T>
void storage<T>::recycle(typename storage<T>::cell_iterator cell) {
    cell->drop_time = time_traits::add(time_traits::now(), idle_timeout_);
    available_.splice(available_.end(), used_, cell);
}

template <class T>
void storage<T>::waste(typename storage<T>::cell_iterator cell) {
    cell->value.reset();
    wasted_.splice(wasted_.end(), used_, cell);
}

} // namespace detail
} // namespace resource_pool
} // namespace yamail
