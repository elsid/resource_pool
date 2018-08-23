list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "CMAKE_CURRENT_LIST_DIR: ${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -std=c++11")

find_package(Boost COMPONENTS coroutine context REQUIRED)

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(GTest)
find_package(GMock)

if(NOT TARGET googletest AND (NOT GTEST_FOUND OR NOT GMOCK_FOUND))
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
        CONFIGURE_COMMAND cmake
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DCMAKE_INSTALL_LIBDIR=${CMAKE_CURRENT_BINARY_DIR}/lib
            ../googletest
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )
endif()

if (GTEST_FOUND AND GMOCK_FOUND)
    include_directories(SYSTEM ${GTEST_INCLUDE_DIRS})
    include_directories(SYSTEM ${GMOCK_INCLUDE_DIRS})
    get_filename_component(_GOOGLETEST_LIB_DIR ${GMOCK_MAIN_LIBRARY} PATH)
else()
    include_directories(SYSTEM ${CMAKE_CURRENT_BINARY_DIR}/include)

    set(_GOOGLETEST_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib)

    set(GTEST_LIBRARIES ${_GOOGLETEST_LIB_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}gtest.a)
    set(GTEST_MAIN_LIBRARY ${_GOOGLETEST_LIB_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_main.a)
    set(GTEST_BOTH_LIBRARIES "${GTEST_LIBRARIES};${GTEST_MAIN_LIBRARY}")

    set(GMOCK_LIBRARIES ${_GOOGLETEST_LIB_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}gmock.a)
    set(GMOCK_MAIN_LIBRARY ${_GOOGLETEST_LIB_DIR}/${CMAKE_FIND_LIBRARY_PREFIXES}gmock_main.a)
    set(GMOCK_BOTH_LIBRARIES "${GMOCK_LIBRARIES};${GMOCK_MAIN_LIBRARY}")
endif()

link_directories(${_GOOGLETEST_LIB_DIR})

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
    rt
    pthread
    gtest
    gmock
    ${RESOURCE_POOL_DEPENDENCY_LIBRARIES}
    ${Boost_LIBRARIES}
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check ctest -V)
endif()

add_test(resource_pool_test resource_pool_test)
add_dependencies(check resource_pool_test)
