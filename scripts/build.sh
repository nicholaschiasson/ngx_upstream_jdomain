#!/usr/bin/env bash

set -ex

BIN_DIR=${GITHUB_WORKSPACE}/bin

echo $BIN_DIR

rm -rf ${BIN_DIR}

mkdir -p ${BIN_DIR}/workdir

pushd ${BIN_DIR}/workdir

NGINX_PACKAGE=/tmp/nginx.tar.gz

curl -L http://nginx.org/download/nginx-${INPUT_NGINXVERSION}.tar.gz -o ${NGINX_PACKAGE}

tar zxvf ${NGINX_PACKAGE} --strip-components 1

./configure \
	--prefix=${BIN_DIR} \
	--with-debug \
	--with-ipv6 \
	--without-http_charset_module \
	--without-http_userid_module \
	--without-http_auth_basic_module \
	--without-http_autoindex_module \
	--without-http_geo_module \
	--without-http_split_clients_module \
	--without-http_referer_module \
	--without-http_fastcgi_module \
	--without-http_uwsgi_module \
	--without-http_scgi_module \
	--without-http_memcached_module \
	--without-http_limit_conn_module \
	--without-http_limit_req_module \
	--without-http_empty_gif_module \
	--without-http_browser_module \
	--without-http_upstream_ip_hash_module \
	--add-module=${GITHUB_WORKSPACE}

make
make install

popd
