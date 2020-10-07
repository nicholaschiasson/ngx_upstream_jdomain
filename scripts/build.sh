#!/usr/bin/env bash

set -ex

source .env

SRC_DIR=/src/nginx
MOD_DIR=/src/modules

pushd ${SRC_DIR}
patch -p1 < ${MOD_DIR}/nginx_upstream_check_module/check_1.16.1+.patch
popd

for type in dynamic static
do
	BIN_DIR=${GITHUB_WORKSPACE}/bin/${type}
	TYPE=${type/static/-}
	TYPE=${TYPE/dynamic/-dynamic-}

	mkdir -p ${BIN_DIR%/*}
	rm -rf ${BIN_DIR}
	pushd ${SRC_DIR}

	./configure \
		--prefix=${BIN_DIR} \
		--with-debug \
		--with-cc-opt='-O0 -g' \
		--with-http_ssl_module \
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
		--add-module=${MOD_DIR}/nginx_upstream_check_module \
		--add-module=${MOD_DIR}/nginx-module-vts \
		--add${TYPE}module=${GITHUB_WORKSPACE} \
		# --with-openssl=/src/openssl

	make

	mv objs ${BIN_DIR}

	popd
done
