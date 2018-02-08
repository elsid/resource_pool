set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -std=c++11")

find_package(Boost COMPONENTS coroutine context REQUIRED)

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

if(NOT TARGET googletest)
    include(ExternalProject)

    set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/external)

    if(NOT GOOGLETEST_REPOSITORY)
        if(NOT DEFINED ENV{GOOGLETEST_REPOSITORY})
            set(GOOGLETEST_REPOSITORY https://github.com/google/googletest.git)
        else()
            set(GOOGLETEST_REPOSITORY $ENV{GOOGLETEST_REPOSITORY})
        endif()
    endif()

    ExternalProject_Add(
        googletest
        GIT_REPOSITORY ${GOOGLETEST_REPOSITORY}
        TIMEOUT 1
        CONFIGURE_COMMAND cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR} ../googletest
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )

    ExternalProject_Get_Property(googletest source_dir)
    include_directories(SYSTEM ${source_dir}/googletest/include)
    include_directories(SYSTEM ${source_dir}/googlemock/include)

    ExternalProject_Get_Property(googletest binary_dir)
    link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
    link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib64)
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
)

add_dependencies(resource_pool_test googletest)

set(LIBRARIES
    -lrt
    -pthread
    gtest
    gmock
    ${RESOURCE_POOL_DEPENDENCY_LIBRARIES}
    ${Boost_LIBRARIES}
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check)
endif()

add_test(resource_pool_test resource_pool_test)
add_dependencies(check resource_pool_test)
