set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -std=c++11")

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)

set(LIBRARIES
    -lrt
    -pthread
    ${RESOURCE_POOL_DEPENDENCY_LIBRARIES}
)

add_executable(async_pool async/pool.cc)
target_link_libraries(async_pool ${LIBRARIES})

add_executable(sync_pool sync/pool.cc)
target_link_libraries(sync_pool ${LIBRARIES})
