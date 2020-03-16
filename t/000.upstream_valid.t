use Test::Nginx::Socket 'no_plan';
run_tests();

__DATA__

=== TEST 1:
--- http_config
resolver 127.0.0.1:1994;
upstream upstream_test {
	server ngx_upstream_jdomain.test port=8000;
	keepalive 32;
}
server {
	listen 127.0.0.1:8000;
	location = / {
		return 200 'Pass';
	}
}
--- config
location = / {
		proxy_pass http://upstream_test;
}
--- request
GET /
--- response_body
Pass
--- error_code: 200
