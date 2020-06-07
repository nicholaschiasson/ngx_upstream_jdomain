use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Invalid upstream
--- init
`echo > /tmp/unbound_local_zone_ngx_upstream_jdomain.conf && unbound-control reload` or die $!;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 retry_off;
}
--- config
--- must_die
--- error_log
host not found in upstream "example.com"
=== TEST 2: Invalid upstream with fallback
--- init
`echo > /tmp/unbound_local_zone_ngx_upstream_jdomain.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 retry_off fallback=127.0.0.3;
}
server {
	listen 127.0.0.3:8000;
	return 999 'Backup';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- error_code: 999
--- response_body: Backup
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:8000"
=== TEST 3: Invalid upstream with fallback specifying port number
--- init
`echo > /tmp/unbound_local_zone_ngx_upstream_jdomain.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 retry_off fallback=127.0.0.3:12345;
}
server {
	listen 127.0.0.3:12345;
	return 999 'Backup';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- error_code: 999
--- response_body: Backup
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:12345"
=== TEST 4: Invalid upstream with fallback but no server
--- init
`echo > /tmp/unbound_local_zone_ngx_upstream_jdomain.conf && unbound-control reload` or die $!;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 retry_off fallback=127.0.0.3:12345;
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- error_code: 502
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:12345"
