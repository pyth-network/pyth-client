#!/usr/bin/env bash
#
# Build given bpf makefile dir (./program by default):
#   ~/pyth-client$ ./scripts/build-bpf.sh
#   ~/pyth-client/program$ ../scripts/build-bpf.sh .
#   ~/$ ./pyth-client/scripts/build-bpf.sh ./serum-pyth/program
#

set -eu

PYTH_DIR=$( cd "${1:-.}" && pwd)

#find the makefile in pyth-client
#ASSUMES THAT there is only one makefile there 
C_DIR="$( find $PYTH_DIR | grep makefile)"
C_DIR=$(dirname $C_DIR)

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi


set -x

#build the C code and make an archive file out of it
cd "${C_DIR}"
export V="${V:-1}"
make clean 
make  "${@:2}" 
make cpyth-bpf 
make cpyth-native
rm ./target/*-keypair.json


#build Rust and link it with C
cd "${PYTH_DIR}"
cargo clean
cargo test-bpf
cargo clean
cargo build-bpf

sha256sum ./target/**/*.so





