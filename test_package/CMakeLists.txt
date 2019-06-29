cmake_minimum_required(VERSION 3.12)
project(PackageTest CXX)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake)
find_package(resource_pool REQUIRED)

add_executable(example ../examples/async/pool.cc)
target_link_libraries(example PRIVATE yandex::resource_pool)
