#!/usr/bin/env bash

set -eu

PYTH_DIR=$( cd "${1:-.}" && pwd)

#find the makefile in pyth-client
#ASSUMES THAT there is only one makefile there 
C_DIR="$( find $PYTH_DIR | grep makefile)"
C_DIR=$(dirname $C_DIR)

set -x

#build the C code and make an archive file out of it
cd "${C_DIR}"
export V=1
make clean 
make
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





