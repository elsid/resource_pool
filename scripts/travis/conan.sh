#!/bin/bash -e

pip install --user virtualenv
virtualenv conan-temp
. conan-temp/bin/activate
pip install conan
conan profile new --detect default

cd "${TRAVIS_BUILD_DIR}"

PKG_REPO="yandex/testing"


conan export . "${PKG_REPO}"

PKG_NAME="$(conan inspect --raw name .)/$(conan inspect --raw version .)"
PKG_ID="${PKG_NAME}@${PKG_REPO}"
echo "package-id: ${PKG_ID}"

conan install "${PKG_ID}" --build=missing
conan test test_package "${PKG_ID}"
