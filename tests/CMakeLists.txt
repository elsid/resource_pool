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
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )

    ExternalProject_Get_Property(googletest source_dir)
    include_directories(SYSTEM ${source_dir}/googletest/include)
    include_directories(SYSTEM ${source_dir}/googlemock/include)

    ExternalProject_Get_Property(googletest binary_dir)
    link_directories(${binary_dir}/googlemock)
endif()

find_package(Boost REQUIRED system thread chrono)

add_executable(resource_pool_test
    main.cc
    error.cc
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
    gmock
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check)
endif()

add_test(resource_pool_test resource_pool_test)
add_dependencies(check resource_pool_test)
