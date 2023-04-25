#!/usr/bin/env bash
#

set -eu

PYTH_DIR=$( cd "${1:-.}" && pwd)

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x

cargo clean
cargo-build-bpf -- -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort

# While Solana doesn't support resizing programs, the oracle binary needs to be smaller than 81760 bytes
# (The available space for the oracle program on pythnet is 88429 and mainnet is 81760)
ORACLE_SIZE=$(wc -c ./target/deploy/pyth_oracle.so | awk '{print $1}')
if [ $ORACLE_SIZE -lt 81760 ]
then
    echo "Size of pyth_oracle.so is small enough to be deployed to mainnet."
    echo $ORACLE_SIZE
else
    echo "Size of pyth_oracle.so is too big to be deployed to mainnet."
    echo $ORACLE_SIZE
    exit 1
fi