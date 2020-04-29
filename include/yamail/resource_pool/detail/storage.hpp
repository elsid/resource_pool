#pragma once

#include <yamail/resource_pool/error.hpp>
#include <yamail/resource_pool/features.hpp>
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
using cell_iterator = typename std::list<idle<T>>::iterator;

template <class T, class ... Features>
class storage {
public:
    using cell_iterator = detail::cell_iterator<T>;

    inline storage(std::size_t capacity, time_traits::duration idle_timeout, time_traits::duration lifespan);

    template <class Generator>
    inline storage(Generator&& generator, std::size_t capacity, time_traits::duration idle_timeout, time_traits::duration lifespan);

    template <class InputIterator>
    inline storage(InputIterator begin, InputIterator end, time_traits::duration idle_timeout, time_traits::duration lifespan);

    storage(const storage& other) = delete;

    storage(storage&& other) = default;

    inline storage_stats stats() const;

    inline boost::optional<cell_iterator> lease();

    inline void recycle(cell_iterator cell);

    inline void waste(cell_iterator cell);

    inline void refresh();

private:
    time_traits::duration idle_timeout_;
    time_traits::duration lifespan_;
    std::list<idle<T>> available_;
    std::list<idle<T>> used_;
    std::list<idle<T>> wasted_;

    time_traits::time_point get_drop_time(time_traits::time_point now) const;
};

template <class CellIterator>
using cell_value = typename CellIterator::value_type::value_type;

template <class T, class ... F>
storage<T, F ...>::storage(std::size_t capacity, time_traits::duration idle_timeout, time_traits::duration lifespan)
        : idle_timeout_(idle_timeout),
          lifespan_(lifespan),
          wasted_(capacity) {
}

template <class T, class ... F>
template <class Generator>
storage<T, F ...>::storage(Generator&& generator, std::size_t capacity, time_traits::duration idle_timeout, time_traits::duration lifespan)
        : idle_timeout_(idle_timeout), lifespan_(lifespan) {
    const auto now = time_traits::now();
    const auto drop_time = get_drop_time(now);
    for (std::size_t i = 0; i < capacity; ++i) {
        available_.emplace_back(generator(), drop_time, now);
    }
}

template <class T, class ... F>
template <class InputIterator>
storage<T, F ...>::storage(InputIterator begin, InputIterator end, time_traits::duration idle_timeout, time_traits::duration lifespan)
        : idle_timeout_(idle_timeout), lifespan_(lifespan) {
    const auto now = time_traits::now();
    const auto drop_time = get_drop_time(now);
    std::for_each(begin, end, [&] (auto&& v) {
        available_.emplace_back(std::forward<decltype(v)>(v), drop_time, now);
    });
}

template <class T, class ... F>
storage_stats storage<T, F ...>::stats() const {
    storage_stats result;
    result.available = available_.size();
    result.used = used_.size();
    result.wasted = wasted_.size();
    return result;
}

template <class T, class ... F>
boost::optional<typename storage<T, F ...>::cell_iterator> storage<T, F ...>::lease() {
    const auto now = time_traits::now();
    while (!available_.empty()) {
        const auto candidate = available_.begin();
        if constexpr (has_feature<features::enable_idle_timeout, F ...>) {
            if (candidate->drop_time > now) {
                used_.splice(used_.end(), available_, candidate);
                return candidate;
            }
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

template <class T, class ... F>
void storage<T, F ...>::recycle(typename storage<T, F ...>::cell_iterator cell) {
    if constexpr (has_feature<features::enable_refresh, F ...>) {
        if (cell->waste_on_recycle) {
            return waste(cell);
        }
    }
    const auto now = time_traits::now();
    if constexpr (has_feature<features::enable_lifespan, F ...>) {
        const auto life_end = time_traits::add(cell->reset_time, lifespan_);
        if (life_end <= now) {
            return waste(cell);
        }
        if constexpr (has_feature<features::enable_idle_timeout, F ...>) {
            cell->drop_time = std::min(time_traits::add(now, idle_timeout_), life_end);
        } else {
            cell->drop_time = life_end;
        }
    } else if constexpr (has_feature<features::enable_idle_timeout, F ...>) {
        cell->drop_time = time_traits::add(now, idle_timeout_);
    }
    available_.splice(available_.end(), used_, cell);
}

template <class T, class ... F>
void storage<T, F ...>::waste(typename storage<T, F ...>::cell_iterator cell) {
    cell->value.reset();
    wasted_.splice(wasted_.end(), used_, cell);
}

template <class T, class ... F>
void storage<T, F ...>::refresh() {
    static_assert(has_feature<features::enable_refresh, F ...>, "enable_refresh feature is disabled");
    if constexpr (has_feature<features::enable_refresh, F ...>) {
        for (auto& cell : available_) {
            cell.value.reset();
        }
        wasted_.splice(wasted_.end(), available_, available_.begin(), available_.end());
        for (auto& cell : used_) {
            cell.waste_on_recycle = true;
        }
    }
}

template <class T, class ... F>
time_traits::time_point storage<T, F ...>::get_drop_time(time_traits::time_point now) const {
    if constexpr (has_feature<features::enable_idle_timeout, F ...> && has_feature<features::enable_lifespan, F ...>) {
        return std::min(time_traits::add(now, idle_timeout_), time_traits::add(now, lifespan_));
    } else if constexpr (has_feature<features::enable_idle_timeout, F ...>) {
        return time_traits::add(now, idle_timeout_);
    } else if constexpr (has_feature<features::enable_lifespan, F ...>) {
        return time_traits::add(now, lifespan_);
    } else {
        (void) now;
        return time_traits::time_point();
    }
}

} // namespace detail
} // namespace resource_pool
} // namespace yamail
