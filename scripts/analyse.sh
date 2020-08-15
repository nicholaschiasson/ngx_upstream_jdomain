#!/usr/bin/env bash

set -ex

source .env

WORK_DIR=${GITHUB_WORKSPACE}/bin/workdir

clang-tidy src/*.c -- \
	-I${WORK_DIR}/src/core \
	-I${WORK_DIR}/src/event \
	-I${WORK_DIR}/src/event/modules \
	-I${WORK_DIR}/src/os/unix \
	-I${WORK_DIR}/objs \
	-I${WORK_DIR}/src/http \
	-I${WORK_DIR}/src/http/modules
