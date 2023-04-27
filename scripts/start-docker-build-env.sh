#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
command -v shellcheck >/dev/null && shellcheck "$0"

dir=$(pwd)
export PYTH_REPO=${dir}
echo "Mounting PYTH_REPO: ${PYTH_REPO}"

# Use the latest oracle image from https://hub.docker.com/r/pythfoundation/pyth-client/tags?name=oracle
LATEST_VERSION="v2.21.0"
export IMAGE=pythfoundation/pyth-client:oracle-${LATEST_VERSION}
echo "Using image: ${IMAGE}"

docker run -it \
        --mount "type=bind,src=${PYTH_REPO},target=/home/pyth/pyth-client" \
        --platform linux/amd64 \
        $IMAGE \
        /bin/bash -l