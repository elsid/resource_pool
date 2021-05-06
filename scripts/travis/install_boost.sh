#!/bin/bash -e

if [ -z "$BOOST_MINOR" ]; then
    echo "No version selected; skipping boost install..."
else
    wget -O "boost_1_${BOOST_MINOR}_0.tar.gz" "https://boostorg.jfrog.io/artifactory/main/release/1.${BOOST_MINOR}.0/source/boost_1_${BOOST_MINOR}_0.tar.gz"
    tar xzf "boost_1_${BOOST_MINOR}_0.tar.gz"
    cd "boost_1_${BOOST_MINOR}_0"
    ./bootstrap.sh --with-libraries=system,thread,context,coroutine,atomic,date_time
    mkdir build
    ./b2 --build-dir=build
    sudo ./b2 --build-dir=build install
    echo "Boost 1.${BOOST_MINOR} installed"
fi
