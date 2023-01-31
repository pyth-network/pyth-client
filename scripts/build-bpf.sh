#!/usr/bin/env bash
#
# Build bpf oracle program :
#   ~/pyth-client$ ./scripts/build-bpf.sh
#   ~/pyth-client/program$ ../scripts/build-bpf.sh

set -eu

PYTH_DIR=$( cd "${1:-.}" && pwd)
C_DIR="$PYTH_DIR/program/c/"

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x

#build the C code and make an archive file out of it
cd "${C_DIR}"
export V="${V:-1}" #verbose flag for solana
make clean 
make
make cpyth-bpf 
make cpyth-native
rm ./target/*-keypair.json


#build Rust and link it with C
cd "${PYTH_DIR}"
# cargo clean
# cargo-test-bpf
cargo clean
cargo-build-bpf -- -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort

sha256sum ./target/**/*.so





