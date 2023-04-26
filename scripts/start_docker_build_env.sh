#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
command -v shellcheck >/dev/null && shellcheck "$0"

dir=$(pwd)
parentdir="$(dirname "$dir")"
export PYTH_REPO=$parentdir

# Use the latest pythd image from https://hub.docker.com/r/pythfoundation/pyth-client/tags?name=pythd
LATEST_VERSION="v2.17.3"
export IMAGE=pythfoundation/pyth-client:pythd-${LATEST_VERSION}

docker run -it \
        --volume "${HOME}:/home/pyth/home" \
        --volume "${HOME}/.config:/home/pyth/.config" \
        --mount "type=bind,src=${PYTH_REPO},target=/home/pyth/pyth-client" \
        --platform linux/amd64 \
        $IMAGE \
        /bin/bash -l