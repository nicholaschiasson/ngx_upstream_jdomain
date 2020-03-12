#!/usr/bin/env bash

set -ex

pushd bin
make
make install
export PATH="/usr/local/nginx/sbin:${PATH}"
cp ../nginx.conf /usr/local/nginx/conf/nginx.conf
nginx -t
popd
