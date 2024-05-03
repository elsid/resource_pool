list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Boost COMPONENTS coroutine context REQUIRED)

if(RESOURCE_POOL_USE_SYSTEM_BENCHMARK)
    find_package(benchmark)
else()
    set(BENCHMARK_ENABLE_TESTING OFF)
    set(BENCHMARK_ENABLE_INSTALL OFF)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF)

    include(FetchContent)
    FetchContent_Declare(benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
        URL_HASH SHA512=d73587ad9c49338749e1d117a6f8c7ff9c603a91a2ffa91a7355c7df7dea82710b9a810d34ddfef20973ecdc77092ec10fb2b4e4cc8d2e7810cbed79617b3828
        SOURCE_DIR fetched/benchmark
    )
    FetchContent_MakeAvailable(benchmark)
endif()

add_executable(resource_pool_benchmark_async async.cc)

set(LIBRARIES
    pthread
    benchmark::benchmark
    Boost::coroutine
    Boost::context
    elsid::resource_pool
)

target_link_libraries(resource_pool_benchmark_async ${LIBRARIES})
