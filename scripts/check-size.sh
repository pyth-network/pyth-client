#!/usr/bin/env bash
#

cargo clean
cargo-build-bpf -- -Z build-std=std,panic_abort -Z build-std-features=panic_immediate_abort

# While Solana doesn't support resizing programs, the oracle binary needs to be smaller than 81760 bytes
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