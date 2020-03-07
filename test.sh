#!/usr/bin/env bash

set -ex

pushd /opt/ngx_upstream_jdomain/bin
make
make install
cp ../nginx.conf /usr/local/nginx/conf/nginx.conf
nginx -t
popd
