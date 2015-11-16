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
    -pthread
    -lgtest
    -lgmock
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

target_link_libraries(resource_pool_test ${LIBRARIES})

if(NOT TARGET check)
    add_custom_target(check COMMAND resource_pool_test)
else()
    add_custom_target(run_resource_pool_test COMMAND resource_pool_test)
    set_property(TARGET run_resource_pool_test PROPERTY EXCLUDE_FROM_ALL TRUE)
    add_dependencies(check run_resource_pool_test)
endif()

add_executable(resource_pool_load_test_async_pool main.cc async/load_pool.cc)

target_link_libraries(resource_pool_load_test_async_pool ${LIBRARIES})

add_executable(resource_pool_load_test_async_request_queue main.cc async/load_request_queue.cc)

target_link_libraries(resource_pool_load_test_async_request_queue ${LIBRARIES})

add_custom_target(resource_pool_load_check
    COMMAND load_test_async_pool
    COMMAND load_test_async_request_queue
)
