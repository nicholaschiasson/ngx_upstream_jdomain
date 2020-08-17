#!/usr/bin/env bash

set -ex

source .env

SRC_DIR=/src/nginx

clang-tidy src/*.c -- \
	-I${SRC_DIR}/src/core \
	-I${SRC_DIR}/src/event \
	-I${SRC_DIR}/src/event/modules \
	-I${SRC_DIR}/src/os/unix \
	-I${SRC_DIR}/objs \
	-I${SRC_DIR}/src/http \
	-I${SRC_DIR}/src/http/modules
