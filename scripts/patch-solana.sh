#!/usr/bin/env bash
#
# Patch BPF makefile and solana_sdk.h:
# - Build with `-Wall -Wextra -Wconversion`.
# - Link with `-z defs`.
# - Add TEST_FLAGS for criterion CLI.
#

set -eu

THIS_DIR="$( dirname "${BASH_SOURCE[0]}" )"
THIS_DIR="$( cd "${THIS_DIR}" && pwd )"
SOLANA="$(dirname $(which cargo-build-bpf))"

set -x
cd "${SOLANA}"
patch -p1 -i "${THIS_DIR}/solana.patch"
