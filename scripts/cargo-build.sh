#!/usr/bin/env bash
#
# build pyth-client rust lib:
#   ~/pyth-client$ ./scripts/cargo-build.sh rust
#
# build a bpf program:
#   ~/$ RUSTUP_TOOLCHAIN=bpf ./scripts/cargo-build.sh path/to/program
#

set -eu

BUILD_DIR="$( cd "${1:-.}" && pwd )"
BUILD_ARGS=( "${@:2}" )

# Build offline by default after building Docker.
# Override with CARGO_NET_OFFLINE=false.
export CARGO_NET_OFFLINE="${CARGO_NET_OFFLINE:-true}"
export CARGO_TERM_VERBOSE="${CARGO_TERM_VERBOSE:-true}"

if ! which cargo &> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x
cd "${BUILD_DIR}"
cargo build "${BUILD_ARGS[@]}"
cargo test

if [ "${RUSTUP_TOOLCHAIN:-}" == "bpf" ]
then
  sha256sum -b target/*/*/*.so
fi
