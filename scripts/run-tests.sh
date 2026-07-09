#!/bin/sh
set -eu
cd "$(dirname "$0")/../src"

rm -rf ../build-test
cmake -S . -B ../build-test \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON
cmake --build ../build-test -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
cd ../build-test
ctest --output-on-failure
