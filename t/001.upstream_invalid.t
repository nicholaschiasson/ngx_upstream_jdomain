use Test::Nginx::Socket 'no_plan';

no_shuffle(); # NO SHUFFLE DUE TO must_die IN TEST 4!
run_tests();

__DATA__

=== TEST 1: Invalid upstream with fallback
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3;
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
=== TEST 2: Invalid SSL upstream with fallback
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3;
}
server {
	listen 127.0.0.3:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 999 'Backup';
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request
GET /
--- error_code: 999
--- response_body: Backup
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:8000"
=== TEST 3: Invalid upstream with fallback specifying port number
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3:12345;
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
=== TEST 4: Invalid SSL upstream with fallback specifying port number
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3:12345;
}
server {
	listen 127.0.0.3:12345 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 999 'Backup';
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request
GET /
--- error_code: 999
--- response_body: Backup
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:12345"
=== TEST 5: Invalid upstream with fallback but no server
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3:12345;
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
=== TEST 6: Invalid SSL upstream with fallback but no server
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3:12345;
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request
GET /
--- error_code: 502
--- error_log
host not found in upstream "example.com", using fallback address "127.0.0.3:12345"
=== TEST 7: Invalid upstream without fallback
THIS TEST MUST NOT BE FIRST! THERE SEEMS TO BE A BUG WITH 'must_die' WHICH
CAUSES THE FRAMEWORK TO HANG IF THIS TEST GOES FIRST!
--- init
`echo > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
--- config
--- must_die
--- error_log
host not found in upstream "example.com"
