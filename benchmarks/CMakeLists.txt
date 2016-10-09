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
        CONFIGURE_COMMAND cmake -DCMAKE_BUILD_TYPE=Release ../google_benchmark
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

find_package(Boost REQUIRED system)

set(LIBRARIES
    -lrt
    -pthread
    benchmark
    ${Boost_SYSTEM_LIBRARY}
)

target_link_libraries(resource_pool_benchmark_async ${LIBRARIES})
