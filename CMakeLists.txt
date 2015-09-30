cmake_minimum_required(VERSION 2.6)

project(resource_pool)

find_package(Boost REQUIRED COMPONENTS system thread chrono)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include)

install(DIRECTORY include/ DESTINATION include COMPONENT development)

add_subdirectory(tests)
