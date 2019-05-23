#!/bin/bash -ex

sudo pip install gcovr
cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_CXX_COMPILER=${CXX_COMPILER} \
    -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -D RESOURCE_POOL_BUILD_TESTS=ON \
    -D RESOURCE_POOL_BUILD_EXAMPLES=ON \
    -D RESOURCE_POOL_BUILD_BENCHMARKS=ON \
    -D RESOURCE_POOL_COVERAGE=ON \
    ${TRAVIS_BUILD_DIR}
make -j $(nproc)
ctest -V -j $(nproc)
