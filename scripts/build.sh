#!/usr/bin/env bash

set -ex

source .env

for type in dynamic static
do
	SRC_DIR=/src/nginx
	BIN_DIR=${GITHUB_WORKSPACE}/bin/${type}
	TYPE=${type/static/-}
	TYPE=${TYPE/dynamic/-dynamic-}

	mkdir -p ${BIN_DIR%/*}
	rm -rf ${BIN_DIR}
	pushd ${SRC_DIR}

	./configure \
		--prefix=${BIN_DIR} \
		--with-debug \
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
		--add${TYPE}module=${GITHUB_WORKSPACE} \
		# --with-http_ssl_module \
		# --with-openssl=/src/openssl

	make

	mv objs ${BIN_DIR}

	popd
done
