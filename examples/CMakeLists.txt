find_package(Boost REQUIRED system)

set(LIBRARIES
    -lrt
    -pthread
    ${Boost_SYSTEM_LIBRARY}
)

add_executable(async_pool async/pool.cc)
target_link_libraries(async_pool ${LIBRARIES})

add_executable(sync_pool sync/pool.cc)
target_link_libraries(sync_pool ${LIBRARIES})
