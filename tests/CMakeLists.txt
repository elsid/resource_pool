find_package(Boost REQUIRED system thread chrono)

set(SOURCES
    main.cc
    sync/pool.cc
    sync/pool_impl.cc
    async/pool.cc
    async/pool_impl.cc
    async/request_queue.cc)

set(LIBRARIES
    -lgtest
    -lgmock
    -pthread
    -lrt
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY})

yamail_add_test(tests "${SOURCES}" "${LIBRARIES}")

add_executable(load_test_async_pool main.cc async/load_pool.cc)

target_link_libraries(load_test_async_pool ${LIBRARIES})

add_executable(load_test_async_request_queue main.cc async/load_request_queue.cc)

target_link_libraries(load_test_async_request_queue ${LIBRARIES})

add_custom_target(load_check
    COMMAND load_test_async_pool
    COMMAND load_test_async_request_queue
)
