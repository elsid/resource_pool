cmake_minimum_required(VERSION 2.6)

project(resource_pool)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -std=c++11")

find_package(Boost REQUIRED)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include)

install(DIRECTORY include/ DESTINATION include COMPONENT development)

enable_testing()

add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(benchmarks)
