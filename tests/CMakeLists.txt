find_package(Boost REQUIRED thread chrono)

set(SOURCES
    main.cc
    sync/pool.cc
    sync/pool_impl.cc
)

add_executable(test EXCLUDE_FROM_ALL ${SOURCES})

target_link_libraries(test
    -lgtest
    -lgmock
    -pthread
    ${Boost_THREAD_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
)

add_custom_target(check
    COMMAND test
)
