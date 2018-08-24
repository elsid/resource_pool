cmake_minimum_required(VERSION 2.6)

project(resource_pool CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_COROUTINES_NO_DEPRECATION_WARNING")

include(./share/find_resource_pool_dependencies.cmake)
find_resource_pool_dependencies()

option(RESOURCE_POOL_BUILD_EXAMPLES OFF)
option(RESOURCE_POOL_BUILD_TESTS OFF)
option(RESOURCE_POOL_BUILD_BENCHMARKS OFF)

add_subdirectory(share)
add_subdirectory(include)

if(RESOURCE_POOL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(RESOURCE_POOL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(RESOURCE_POOL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
