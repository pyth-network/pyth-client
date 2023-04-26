#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
command -v shellcheck >/dev/null && shellcheck "$0"

dir=$(pwd)
parentdir="$(dirname "$dir")"
export PYTH_REPO=$parentdir

# Use the latest oracle image from https://hub.docker.com/r/pythfoundation/pyth-client/tags?name=oracle
LATEST_VERSION="v2.21.0"
export IMAGE=pythfoundation/pyth-client:oracle-${LATEST_VERSION}

docker run -it \
        --mount "type=bind,src=${PYTH_REPO},target=/home/pyth/pyth-client" \
        --platform linux/amd64 \
        $IMAGE \
        /bin/bash -l