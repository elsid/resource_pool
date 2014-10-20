set(SOURCES
    main.cc
    pool.cc
    pool_impl.cc
)

add_executable(test EXCLUDE_FROM_ALL ${SOURCES})

target_link_libraries(test
    -lgtest
    -lgmock
    -pthread
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
)

add_custom_target(check
    COMMAND test
)
