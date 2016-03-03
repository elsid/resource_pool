find_package(Boost REQUIRED system thread chrono)

add_executable(resource_pool_test
    main.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/request_queue.cc
)

set(LIBRARIES
    -lrt
    -pthread
    -lgtest
    -lgmock
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check)
endif()

enable_testing()
add_test(resource_pool_test resource_pool_test)
add_dependencies(check resource_pool_test)
