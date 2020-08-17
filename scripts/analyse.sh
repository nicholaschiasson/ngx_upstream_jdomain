#!/usr/bin/env bash

set -ex

source .env

BIN_DIR=${GITHUB_WORKSPACE}/bin

clang-tidy src/*.c -- \
	-I${BIN_DIR}/workdir/src/core \
	-I${BIN_DIR}/workdir/src/event \
	-I${BIN_DIR}/workdir/src/event/modules \
	-I${BIN_DIR}/workdir/src/os/unix \
	-I${BIN_DIR}/static \
	-I${BIN_DIR}/workdir/src/http \
	-I${BIN_DIR}/workdir/src/http/modules
