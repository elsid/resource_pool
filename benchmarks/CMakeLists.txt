set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror -std=c++14")

include_directories(SYSTEM ${RESOURCE_POOL_DEPENDENCY_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)

if(NOT TARGET google_benchmark)
    include(ExternalProject)

    set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/external)

    if(NOT GOOGLE_BENCHMARK_REPOSITORY)
        if(NOT DEFINED ENV{GOOGLE_BENCHMARK_REPOSITORY})
            set(GOOGLE_BENCHMARK_REPOSITORY https://github.com/google/benchmark.git)
        else()
            set(GOOGLE_BENCHMARK_REPOSITORY $ENV{GOOGLE_BENCHMARK_REPOSITORY})
        endif()
    endif()

    ExternalProject_Add(
        google_benchmark
        GIT_REPOSITORY ${GOOGLE_BENCHMARK_REPOSITORY}
        TIMEOUT 1
        CONFIGURE_COMMAND cmake -DCMAKE_BUILD_TYPE=Release -DBENCHMARK_ENABLE_GTEST_TESTS=OFF ../google_benchmark
        INSTALL_COMMAND ""
        UPDATE_COMMAND ""
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
    )

    ExternalProject_Get_Property(google_benchmark source_dir)
    include_directories(SYSTEM ${source_dir}/include)

    ExternalProject_Get_Property(google_benchmark binary_dir)
    link_directories(${binary_dir}/src)
endif()

add_executable(resource_pool_benchmark_async async.cc)
add_dependencies(resource_pool_benchmark_async google_benchmark)

set(LIBRARIES
    rt
    pthread
    benchmark
    ${RESOURCE_POOL_DEPENDENCY_LIBRARIES}
)

target_link_libraries(resource_pool_benchmark_async ${LIBRARIES})
