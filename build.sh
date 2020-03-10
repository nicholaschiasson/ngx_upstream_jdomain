#!/usr/bin/env bash

set -ex

rm -rf bin
mkdir -p bin

pushd bin;

NGINX_VERSION=${NGINX_VERSION:-1.16.1}
NGINX_PACKAGE=/tmp/nginx.tar.gz

curl -L http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz -o ${NGINX_PACKAGE}

tar zxvf ${NGINX_PACKAGE} --strip-components 1

./configure \
	--with-http_ssl_module \
	--add-module=..

make

make install

cp ../nginx.conf /usr/local/nginx/conf/nginx.conf

ln -sf /usr/local/nginx/sbin/nginx /usr/local/sbin/nginx

popd
