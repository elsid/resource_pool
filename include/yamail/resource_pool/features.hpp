#pragma once

#include <type_traits>

namespace yamail::resource_pool {
namespace features {

struct enable_idle_timeout {};
struct enable_refresh {};
struct enable_lifespan {};

} // namespace features

template <class Feature, class ... Features>
static constexpr bool has_feature = has_feature<Feature, Features ...>;

template <class Feature, class Head, class ... Tail>
static constexpr bool has_feature<Feature, Head, Tail ...> = std::is_same_v<std::decay_t<Feature>, std::decay_t<Head>>
        || has_feature<Feature, Tail ...>;

template <class Feature, class Head>
static constexpr bool has_feature<Feature, Head> = std::is_same_v<std::decay_t<Feature>, std::decay_t<Head>>;

template <class Feature>
static constexpr bool has_feature<Feature> = false;

} // namespace yamail::resource_pool
