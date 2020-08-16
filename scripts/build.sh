#!/usr/bin/env bash

set -ex

source .env

NGINX_PACKAGE=/tmp/nginx.tar.gz

curl -L http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz -o ${NGINX_PACKAGE}

for type in dynamic static
do
	BIN_DIR=${GITHUB_WORKSPACE}/bin/${type}
	WORK_DIR=${GITHUB_WORKSPACE}/bin/workdir
	TYPE=${type/static/-}
	TYPE=${TYPE/dynamic/-dynamic-}

	rm -rf ${BIN_DIR}
	mkdir -p ${WORK_DIR}
	pushd ${WORK_DIR}

	tar zxvf ${NGINX_PACKAGE} --strip-components 1

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
		--add${TYPE}module=${GITHUB_WORKSPACE}

	make
	make install

	popd
done
