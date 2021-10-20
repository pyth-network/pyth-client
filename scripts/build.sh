#!/bin/bash

set -eu

if [[ $# -ge 1 ]]
then
  BUILD_DIR="$1"
  shift
else
  THIS_DIR="$( dirname "${BASH_SOURCE[0]}" )"
  THIS_DIR="$( cd "${THIS_DIR}" && pwd )"
  ROOT_DIR="$( dirname "${THIS_DIR}" )"
  BUILD_DIR="${ROOT_DIR}/build"
fi

ROOT_DIR="$( mkdir -p "${BUILD_DIR}" && cd "${BUILD_DIR}/.." && pwd )"
BUILD_DIR="$( realpath --relative-to="${ROOT_DIR}" "${BUILD_DIR}" )"

if [[ ! -v CMAKE_ARGS && $BUILD_DIR == *debug* ]]
then
  CMAKE_ARGS=( "-DCMAKE_BUILD_TYPE=Debug" )
fi

set -x

cd "${ROOT_DIR}"
rm -rf "${BUILD_DIR}"
mkdir "${BUILD_DIR}"

cd "${BUILD_DIR}"
cmake ${CMAKE_ARGS[@]+"${CMAKE_ARGS[@]}"} ..
make "$@"
ctest
