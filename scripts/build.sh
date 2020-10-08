#!/usr/bin/env bash

set -ex

source .env

SRC_DIR=/src/nginx
MOD_DIR=/src/modules

NGINX_VERSION=$(cat ${SRC_DIR}/src/core/nginx.h | grep "#define NGINX_VERSION" | cut -d\" -f2)
PATCH_VERSION=$(ls ${MOD_DIR}/nginx_upstream_check_module/check_*.patch | sort -Vr | while read f
do
	VERS=${f##*_}
	VERS=${VERS%.*}
	VERS=${VERS%%+*}
	NGINX_MAJ=$(echo $NGINX_VERSION | cut -d. -f1)
	NGINX_MIN=$(echo $NGINX_VERSION | cut -d. -f2)
	NGINX_PAT=$(echo $NGINX_VERSION | cut -d. -f3)
	PATCH_MAJ=$(echo $VERS | cut -d. -f1)
	PATCH_MIN=$(echo $VERS | cut -d. -f2)
	PATCH_PAT=$(echo $VERS | cut -d. -f3)
	if [ "$NGINX_MAJ" -gt "$PATCH_MAJ" ]
	then
		echo $VERS
		break
	elif [ "$NGINX_MAJ" -eq "$PATCH_MAJ" ]
	then
		if [ "$NGINX_MIN" -gt "$PATCH_MIN" ]
		then
			echo $VERS
			break
		elif [ "$NGINX_MIN" -eq "$PATCH_MIN" ]
		then
			if [ "$NGINX_PAT" -ge "$PATCH_PAT" ]
			then
				echo $VERS
				break
			fi
		fi
	fi
done)

pushd ${SRC_DIR}
patch -p1 < ${MOD_DIR}/nginx_upstream_check_module/check_${PATCH_VERSION}+.patch
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
