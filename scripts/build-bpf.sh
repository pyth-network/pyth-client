#!/usr/bin/env bash
#
# Build given bpf makefile dir (./program by default):
#   ~/pyth-client$ ./scripts/build-bpf.sh
#   ~/pyth-client/program$ ../scripts/build-bpf.sh .
#   ~/$ ./pyth-client/scripts/build-bpf.sh ./serum-pyth/program
#

set -eu

C_DIR="$( cd "${1:-.}" && pwd )"

if [[ ! -f "${C_DIR}/makefile" ]]
then
  if [[ -f "${C_DIR}/program/makefile" ]]
  then
    C_DIR="${C_DIR}/program"
  else
    >&2 echo "Not a makefile dir: ${C_DIR}"
    exit 1
  fi
fi


RUST_DIR="$( cd "${1:-.}" && pwd )"

if [[ ! -f "${RUST_DIR}/cargo.toml" ]]
then
  if [[ -f "${RUST_DIR}/rust_entrypoint/Cargo.toml" ]]
  then
    RUST_DIR="${RUST_DIR}/rust_entrypoint"
  else
    >&2 echo "Not a rust dir: ${RUST_DIR}"
    exit 1
  fi
fi

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi


set -x


cd "${C_DIR}"
export V="${V:-1}"
make clean
make  "${@:2}"
make cpyth
rm ./target/*-keypair.json



cd "${RUST_DIR}"
cargo clean
cargo build-bpf
sha256sum ./target/**/*.so
rm ./target/**/*-keypair.json
rm -r ../target || true
mv ./target ..




