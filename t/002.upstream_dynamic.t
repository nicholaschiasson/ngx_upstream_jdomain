use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Dynamic upstream
--- init
open(FH, '>', '/tmp/unbound_local_zone_ngx_upstream_jdomain.conf') or die $!;
print FH 'local-data: "example.com 1 A 127.0.0.2"';
close(FH);
`unbound-control reload`;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000;
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
