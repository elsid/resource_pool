cmake_minimum_required(VERSION 2.6)

project(resource_pool)

find_package(Boost REQUIRED)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include)

install(DIRECTORY include/ DESTINATION include COMPONENT development)

enable_testing()

add_subdirectory(tests)
