#!/usr/bin/env bash

set -ex

source .env

export OLD_PATH=${PATH}

echo "nameserver 127.0.0.88" > /etc/resolv.conf

unbound-control -c /tmp/unbound.conf start
unbound-control verbosity 3

export TEST_NGINX_USE_VALGRIND=1
# export TEST_NGINX_CHECK_LEAK=1

DYNAMIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/dynamic
STATIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/static

chmod 755 ${DYNAMIC_BIN_DIR}/nginx ${STATIC_BIN_DIR}/nginx

# We run prove twice: once with statically linked module and once with dynamically linked module.
PATH=${STATIC_BIN_DIR}:${OLD_PATH} prove -rv t/
PATH=${DYNAMIC_BIN_DIR}:${OLD_PATH} TEST_NGINX_LOAD_MODULES=${DYNAMIC_BIN_DIR}/ngx_http_upstream_jdomain_module.so prove -rv t/
