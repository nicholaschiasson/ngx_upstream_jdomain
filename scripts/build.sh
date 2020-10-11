#!/usr/bin/env bash

set -ex

function get_compatible_version {
	nginx_version=$1
	version_list=${@:2}
	nginx_maj=$(echo $nginx_version | cut -d. -f1)
	nginx_min=$(echo $nginx_version | cut -d. -f2)
	nginx_pat=$(echo $nginx_version | cut -d. -f3)
	for v in $version_list
	do
		echo $v
	done | sort -Vr | while read v
	do
		patch_maj=$(echo $v | cut -d. -f1)
		patch_min=$(echo $v | cut -d. -f2)
		patch_pat=$(echo $v | cut -d. -f3)
		if [ "$nginx_maj" -gt "$patch_maj" ]
		then
			echo $v
			break
		elif [ "$nginx_maj" -eq "$patch_maj" ]
		then
			if [ "$nginx_min" -gt "$patch_min" ]
			then
				echo $v
				break
			elif [ "$nginx_min" -eq "$patch_min" ]
			then
				if [ "$nginx_pat" -ge "$patch_pat" ]
				then
					echo $v
					break
				fi
			fi
		fi
	done
}

source .env

SRC_DIR=/src/nginx
PAT_DIR=/src/patches
MOD_DIR=/src/modules

NGINX_VERSION=$(cat ${SRC_DIR}/src/core/nginx.h | grep "#define NGINX_VERSION" | cut -d\" -f2)
NOPOOL_PATCH_VERSION=$(get_compatible_version ${NGINX_VERSION} $(ls ${PAT_DIR}/no-pool-nginx/*.patch | grep -oE [0-9]+\.[0-9]+\.[0-9]+))
CHECK_PATCH_VERSION=$(get_compatible_version ${NGINX_VERSION} $(ls ${MOD_DIR}/nginx_upstream_check_module/check*.patch | grep -oE [0-9]+\.[0-9]+\.[0-9]+))

pushd ${SRC_DIR}
# sed -i.bak "s/#define nginx_version.*/$(cat ${SRC_DIR}/src/core/nginx.h | grep "#define nginx_version")/" ${PAT_DIR}/no-pool-nginx/nginx-${NOPOOL_PATCH_VERSION}-no_pool.patch
# sed -i.bak "s/${NOPOOL_PATCH_VERSION}/${NGINX_VERSION}/" ${PAT_DIR}/no-pool-nginx/nginx-${NOPOOL_PATCH_VERSION}-no_pool.patch
# patch -p1 < ${PAT_DIR}/no-pool-nginx/nginx-${NOPOOL_PATCH_VERSION}-no_pool.patch
patch -p1 < ${MOD_DIR}/nginx_upstream_check_module/check_${CHECK_PATCH_VERSION}+.patch
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
