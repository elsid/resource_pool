cmake_minimum_required(VERSION 2.6)

project(resource_pool)

include(./share/find_resource_pool_dependencies.cmake)
find_resource_pool_dependencies()

enable_testing()

add_subdirectory(share)
add_subdirectory(include)
add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(benchmarks)
