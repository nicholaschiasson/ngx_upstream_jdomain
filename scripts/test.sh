#!/usr/bin/env bash

set -ex

source .env

mkdir -p /etc/ssl/nginx/test
openssl req -x509 -newkey rsa:4096 -keyout /etc/ssl/nginx/test/key.pem -out /etc/ssl/nginx/test/cert.pem -days 1 -nodes -subj "/C=/ST=/L=/O=/OU=/CN=example.com"

echo "nameserver 127.0.0.88" > /etc/resolv.conf

unbound-control -c /etc/unbound/unbound.conf start
unbound-control verbosity 3

export TEST_NGINX_USE_VALGRIND=1

DYNAMIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/dynamic
STATIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/static

chmod 755 ${DYNAMIC_BIN_DIR}/nginx ${STATIC_BIN_DIR}/nginx

# We run prove twice: once with statically linked module and once with dynamically linked module.
PATH=${STATIC_BIN_DIR}:${PATH} prove -rv t/
PATH=${DYNAMIC_BIN_DIR}:${PATH} TEST_NGINX_LOAD_MODULES=${DYNAMIC_BIN_DIR}/ngx_http_upstream_jdomain_module.so prove -rv t/
