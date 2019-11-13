#!/bin/bash -ex

pip install --user virtualenv
virtualenv conan-temp
. conan-temp/bin/activate
pip install conan
conan profile new --detect default

export CC=/usr/bin/clang-8
export CXX=/usr/bin/clang++-8

conan profile update settings.compiler=clang default
conan profile update settings.compiler.version=8 default
conan profile update settings.compiler.libcxx=libstdc++11 default

cd "${TRAVIS_BUILD_DIR}"

PKG_REPO="elsid/testing"
conan export . "${PKG_REPO}"

PKG_NAME="$(conan inspect --raw name .)/$(conan inspect --raw version .)"
PKG_ID="${PKG_NAME}@${PKG_REPO}"
echo "package-id: ${PKG_ID}"

conan install "${PKG_ID}" --build=missing
conan test test_package "${PKG_ID}"
