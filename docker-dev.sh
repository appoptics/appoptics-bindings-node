#!/bin/bash

# note: alpine has no bash
#
# to run the image:
# ./docker_dev image
#
# Can use specific tags:
# ./docker_dev 
# ./docker_dev 

os_node=${1:-'node:14-buster'} # when add support to 16 change to 'node'

cleanup() {
    # remove artifacts left locally by previous npm install
    rm -rf node_modules 
    rm -rf dist
    rm -rf build-tmp-napi-v7
    rm -rf build-tmp-napi-v4

    trap 'exit 0' EXIT
}

set -e
trap cleanup EXIT

# pull a standard image
docker pull "$os_node"

# open a shell in detached mode
container_id=$(docker run -itd \
    --hostname "${os_node}" \
    --privileged \
    --workdir /usr/src/work \
    -v "$(pwd)":/usr/src/work \
    -v ~/.gitconfig:/root/.gitconfig \
    --env-file .env \
    "$os_node" bash)

# install what is needed
docker exec "$container_id" bash -c "apt-get update && apt-get -y install nano"
docker exec "$container_id" bash -c "npm install --unsafe-perm"

# ready for work
docker attach "$container_id"
