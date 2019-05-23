#!/bin/bash -ex

cmake \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_CXX_COMPILER=${CXX_COMPILER} \
    -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -D RESOURCE_POOL_BUILD_TESTS=ON \
    -D RESOURCE_POOL_BUILD_EXAMPLES=ON \
    -D RESOURCE_POOL_BUILD_BENCHMARKS=ON \
    ${TRAVIS_BUILD_DIR}
make -j $(nproc)
ctest -V -j $(nproc)
benchmarks/resource_pool_benchmark_async
examples/async_pool
examples/async_strand
examples/coro_pool
examples/sync_pool
