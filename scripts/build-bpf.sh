#!/usr/bin/env bash
#
# Build given bpf makefile dir (./program by default):
#   ~/pyth-client$ ./scripts/build-bpf.sh
#   ~/pyth-client/program$ ../scripts/build-bpf.sh .
#   ~/$ ./pyth-client/scripts/build-bpf.sh ./serum-pyth/program
#

set -eu

BUILD_DIR="$( cd "${1:-.}" && pwd )"

if [[ ! -f "${BUILD_DIR}/makefile" ]]
then
  if [[ -f "${BUILD_DIR}/program/makefile" ]]
  then
    BUILD_DIR="${BUILD_DIR}/program"
  else
    >&2 echo "Not a makefile dir: ${BUILD_DIR}"
    exit 1
  fi
fi

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x

cd "${BUILD_DIR}"
export V="${V:-1}"
make clean
make "${@:2}"
sha256sum ../target/*.so
rm ../target/*-keypair.json
