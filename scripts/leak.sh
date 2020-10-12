#!/usr/bin/env bash

set -ex

source .env

mkdir -p /etc/ssl/nginx/test
openssl req -x509 -newkey rsa:4096 -keyout /etc/ssl/nginx/test/key.pem -out /etc/ssl/nginx/test/cert.pem -days 1 -nodes -subj "/C=/ST=/L=/O=/OU=/CN=example.com"

echo "nameserver 127.0.0.88" > /etc/resolv.conf

unbound-control -c /etc/unbound/unbound.conf start
unbound-control verbosity 3

export TEST_NGINX_CHECK_LEAK=1
export TEST_NGINX_CHECK_LEAK_COUNT=100

DYNAMIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/dynamic
STATIC_BIN_DIR=${GITHUB_WORKSPACE}/bin/static

chmod 755 ${DYNAMIC_BIN_DIR}/nginx ${STATIC_BIN_DIR}/nginx

# We run prove twice: once with statically linked module and once with dynamically linked module.
ls t/*.t | while read f
do
	echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload
	echo 'local-data: "example.ca 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf && unbound-control reload
	PATH=${STATIC_BIN_DIR}:${PATH} prove -rv ${f}
	echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload
	echo 'local-data: "example.ca 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf && unbound-control reload
	PATH=${DYNAMIC_BIN_DIR}:${PATH} TEST_NGINX_LOAD_MODULES=${DYNAMIC_BIN_DIR}/ngx_http_upstream_jdomain_module.so prove -rv ${f}
done
