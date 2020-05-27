#!/usr/bin/env bash

set -ex

export PATH="${GITHUB_WORKSPACE}/bin/sbin:${PATH}"

echo "nameserver 127.0.0.88" > /etc/resolv.conf

unbound-control -c /tmp/unbound.conf start
unbound-control verbosity 3

export TEST_NGINX_USE_VALGRIND=1
# export TEST_NGINX_CHECK_LEAK=1

prove -rv t/ || true
