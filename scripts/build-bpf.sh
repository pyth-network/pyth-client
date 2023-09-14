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

cargo test --locked

# Re-run tests affected by features
cargo test --locked --features pythnet -- \
      test_upd_price \
      test_upd_price_v2 \
      test_upd_price_no_fail_on_error

cargo test --locked --features price_v2_resize -- \
      test_publish \
      test_resize_price_account \
      --skip test_publish_batch # This one is not relevant but still fuzzy-matched by the runner

cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort
sha256sum ./target/**/*.so
echo "Checking size of pyth_oracle.so for mainnet"
./scripts/check-size.sh 81760

cargo-build-bpf -- --locked -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort --features pythnet
sha256sum ./target/**/*.so
echo "Checking size of pyth_oracle.so for pythnet"
./scripts/check-size.sh 88429
