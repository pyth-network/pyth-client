# The goal of this script is downloading our own version of bpf/sbf tools. When cargo-build-bpf gets called
# https://github.com/solana-labs/sbf-tools/releases/download/v1.29/solana-bpf-tools-linux.tar.bz2 get downloaded extracted and linked.
# This script is meant to override that behavior and use another version of bpf-tools.
# Using a custom version of bpf-tools allows our use of -Z build-std=std,panic_abort flags that make the binary smaller
# If solana fixes bpf-tools, we can revert back to the default behavior (PR : https://github.com/solana-labs/cargo/pull/1)
curl https://github.com/guibescos/sbf-tools/releases/download/v1.29.1/solana-bpf-tools-linux.tar.bz2 --output solana-bpf-tools-linux.tar.bz2 -L
mkdir -p ~/.cache/solana/v1.29/bpf-tools/
tar -xf solana-bpf-tools-linux.tar.bz2 -C ~/.cache/solana/v1.29/bpf-tools/
rm solana-bpf-tools-linux.tar.bz2
rustup toolchain link bpf ~/.cache/solana/v1.29/bpf-tools/rust