cmake_minimum_required(VERSION 2.6)

project(resource_pool)

find_package(Boost 1.53 REQUIRED COMPONENTS system thread)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

add_subdirectory(tests)

install(DIRECTORY include/ DESTINATION include COMPONENT development)
