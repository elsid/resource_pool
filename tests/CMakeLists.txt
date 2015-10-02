set(LIBRARIES
    -lgtest
    -lgmock
    -pthread
    -lrt
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

if(NOT TARGET check)
    add_custom_target(check)
endif()

add_executable(test
    main.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/request_queue.cc
)
target_link_libraries(test ${LIBRARIES})
add_test(test test)
add_dependencies(check test)

add_executable(load_test_async_pool
    main.cc
    async/load_pool.cc
)
target_link_libraries(load_test_async_pool ${LIBRARIES})
add_test(load_test_async_pool load_test_async_pool)
add_dependencies(check load_test_async_pool)

add_executable(load_test_async_request_queue
    main.cc
    async/load_request_queue.cc
)
target_link_libraries(load_test_async_request_queue ${LIBRARIES})
add_test(load_test_async_request_queue load_test_async_request_queue)
add_dependencies(check load_test_async_request_queue)
