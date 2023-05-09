#!/usr/bin/env bash
#
# Build bpf oracle program :
#   ~/pyth-client$ ./scripts/build-bpf.sh
#   ~/pyth-client/program$ ../scripts/build-bpf.sh

set -eu

PYTH_DIR=$( cd "${1:-.}" && pwd)

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x

# Build both parts of the program (both C and rust) via Cargo
cd "${PYTH_DIR}"
cargo clean
cargo test --locked
cargo clean
cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort
./scripts/check-size.sh 81760
cargo clean
cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort --features pythnet
./scripts/check-size.sh 88429
