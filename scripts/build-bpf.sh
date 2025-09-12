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

cargo-test-bpf
cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort
sha256sum ./target/**/*.so
echo "Checking size of pyth_oracle.so for pythnet"
./scripts/check-size.sh 88429
mkdir -p target/pyth/pythnet/
mv target/deploy/pyth_oracle.so target/pyth/pythnet/pyth_oracle_pythnet.so

# Re-run tests affected by features
cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort --features no-default-accumulator-v2
cargo test --locked --features no-default-accumulator-v2
sha256sum ./target/**/*.so
echo "Checking size of pyth_oracle.so for pythnet with no accumulator"
./scripts/check-size.sh 88429
mkdir -p target/pyth/pythnet/
mv target/deploy/pyth_oracle.so target/pyth/pythnet/pyth_oracle_pythnet_no_accumulator_v2.so
