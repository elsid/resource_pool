list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")

find_package(Boost COMPONENTS coroutine context REQUIRED)

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

if(RESOURCE_POOL_USE_SYSTEM_GOOGLETEST)
    find_package(GTest)
    find_package(GMock)
else()
    include(FetchContent)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
        URL_HASH SHA512=1479ea2f3172c622c0ca305f5b2bc45a42941221ec0ac7865e6d6d020ec4d008d952fc64e01a4c5138d7bed4148cf75596f25bb9e9044a98bbbf5662053ea11c
        SOURCE_DIR fetched/googletest
    )
    FetchContent_MakeAvailable(googletest)

    add_library(GTest::GTest ALIAS gtest)
    add_library(GMock::GMock ALIAS gmock)
endif()

add_executable(resource_pool_test
    main.cc
    error.cc
    handle.cc
    time_traits.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/queue.cc
    async/integration.cc
)

set(LIBRARIES
    pthread
    GTest::GTest
    GMock::GMock
    Boost::coroutine
    Boost::context
    elsid::resource_pool
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check ctest -V)
endif()

add_test(resource_pool_test resource_pool_test)
add_dependencies(check resource_pool_test)

option(RESOURCE_POOL_COVERAGE "Check coverage" OFF)

if(RESOURCE_POOL_COVERAGE AND CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
    set(COVERAGE_EXCLUDES "'.*/(tests|contrib|examples|gmock|gtest)/.*'")
    SETUP_TARGET_FOR_COVERAGE_GCOVR(
        NAME resource_pool_coverage
        EXECUTABLE ctest
        DEPENDENCIES resource_pool_test
    )
endif()
