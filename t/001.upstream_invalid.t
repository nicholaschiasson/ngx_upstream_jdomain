use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Invalid upstream
--- init
open(FH, '>', '/tmp/unbound_local_zone_ngx_upstream_jdomain.conf') or die $!;
print FH '';
close(FH);
`unbound-control reload`;
--- http_config
upstream upstream_test {
	jdomain example.com port=8000 fallback=127.0.0.3:12345;
}
server {
	listen 127.0.0.2:8000;
	return 200 'Pass';
}
server {
	listen 127.0.0.3:12345;
	return 502 'Backup';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- error_code: 502
--- response_body: Backup
