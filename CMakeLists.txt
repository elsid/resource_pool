cmake_minimum_required(VERSION 3.12)
project(resource_pool VERSION 0.1.0 LANGUAGES CXX)

option(RESOURCE_POOL_BUILD_EXAMPLES OFF)
option(RESOURCE_POOL_BUILD_TESTS OFF)
option(RESOURCE_POOL_BUILD_BENCHMARKS OFF)
option(RESOURCE_POOL_USE_SYSTEM_GOOGLETEST OFF)
option(RESOURCE_POOL_USE_SYSTEM_BENCHMARK OFF)

add_subdirectory(include)

if(RESOURCE_POOL_BUILD_TESTS)
    enable_testing()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
    add_subdirectory(tests)
endif()

if(RESOURCE_POOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(RESOURCE_POOL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
