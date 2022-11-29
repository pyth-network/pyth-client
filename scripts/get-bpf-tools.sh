#!/usr/bin/env bash
#
# The goal of this script is downloading our own version of bpf/sbf tools. When cargo-build-bpf/sbf gets called
# https://github.com/solana-labs/sbf-tools/releases/download/v1.29/solana-bpf-tools-linux.tar.bz2 gets downloaded, extracted and linked as a toolchain.
# This script is meant to override that behavior and use another version of bpf/sbf-tools.
# Using a custom version of bpf/sbf-tools allows our use of -Z build-std=std,panic_abort flags that make the binary smaller. These flags compile the std library from scratch.
# If solana fixes bpf/sbf-tools, we can revert back to the default behavior (once this PR https://github.com/solana-labs/cargo/pull/1 gets merged and released)
set -eux

curl https://github.com/guibescos/sbf-tools/releases/download/v1.29.1/solana-bpf-tools-linux.tar.bz2 --output solana-bpf-tools-linux.tar.bz2 -L
mkdir -p ~/.cache/solana/v1.29/bpf-tools/
tar -xf solana-bpf-tools-linux.tar.bz2 -C ~/.cache/solana/v1.29/bpf-tools/
rm solana-bpf-tools-linux.tar.bz2

# Source cargo
if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

# Link the toolchain for future use
rustup toolchain link bpf ~/.cache/solana/v1.29/bpf-tools/rust