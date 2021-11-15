#!/usr/bin/env bash
#
# release build:
#   ~/pyth-client$ ./scripts/build.sh [build]
#
# debug build:
#   ~/pyth-client$ ./scripts/build.sh debug
#
# build other project:
#   ~/pyth-client$ ./scripts/build.sh ../serum-pyth/build
#   ~/serum-pyth$ ../pyth-client/scripts/build.sh
#

set -eu

if [[ $# -ge 1 ]]
then
  BUILD_DIR="$1"
  shift
else
  BUILD_DIR="build"
fi

ROOT_DIR="$( mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}/.." && pwd )"
BUILD_DIR="$( cd "${BUILD_DIR}" && pwd )"
BUILD_DIR="$( basename "${BUILD_DIR}" )"

if [[ ! -v CMAKE_ARGS && $BUILD_DIR == *debug* ]]
then
  CMAKE_ARGS=( "-DCMAKE_BUILD_TYPE=Debug" )
fi

# Check before rm -rf $BUILD_DIR
if [[ ! -f "${ROOT_DIR}/CMakeLists.txt" ]]
then
  >&2 echo "Not a cmake project dir: ${ROOT_DIR}"
  exit 1
fi

set -x

cd "${ROOT_DIR}"
rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"

cd "${BUILD_DIR}"
cmake ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"} ..
make "$@"
ctest
