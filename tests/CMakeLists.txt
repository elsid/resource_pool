find_package(Boost REQUIRED system thread chrono)

add_executable(test
    main.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/request_queue.cc)

target_link_libraries(test
    -lgtest
    -lgmock
    -pthread
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

add_executable(load_test_async_pool main.cc async/load_pool.cc)

target_link_libraries(load_test_async_pool
    -lgtest
    -lgmock
    -pthread
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

add_executable(load_test_async_request_queue main.cc async/load_request_queue.cc)

target_link_libraries(load_test_async_request_queue
    -lgtest
    -lgmock
    -pthread
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

add_custom_target(check
    COMMAND test
    COMMAND load_test_async_pool
    COMMAND load_test_async_request_queue
)
