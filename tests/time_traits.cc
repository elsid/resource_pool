#include <yamail/resource_pool/time_traits.hpp>

#include <gtest/gtest.h>

namespace {

using namespace testing;
using namespace yamail::resource_pool;

struct time_traits_test : Test {};

TEST(time_traits_test, add_more_than_max_should_return_max) {
    time_traits::time_point result = time_traits::add(time_traits::time_point::max(), time_traits::duration::max());
    EXPECT_EQ(result, time_traits::time_point::max());
}

TEST(time_traits_test, add_to_less_than_epoch_should_return_min) {
    time_traits::time_point result = time_traits::add(time_traits::time_point::min(), time_traits::duration::min());
    EXPECT_EQ(result, time_traits::time_point(time_traits::duration::min()));
}

TEST(time_traits_test, add_epoch_should_return_increased) {
    time_traits::time_point result = time_traits::add(time_traits::time_point(), time_traits::duration(1));
    EXPECT_EQ(result, time_traits::time_point(time_traits::duration(1)));
}

}
