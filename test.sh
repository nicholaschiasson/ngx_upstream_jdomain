#!/usr/bin/env bash

set -ex

pushd bin
make
make install
cp ../nginx.conf /usr/local/nginx/conf/nginx.conf
nginx -t
popd
