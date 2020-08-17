use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Valid upstream
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 retry_off;
}
server {
	listen 127.0.0.2:8000;
	return 200 'Pass';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- response_body: Pass
=== TEST 2: Valid upstream no server
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 retry_off;
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- error_code: 502
