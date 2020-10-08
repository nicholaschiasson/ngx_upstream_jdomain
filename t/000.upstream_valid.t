use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Valid upstream
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
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
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[200, 200, 200, 200, 200, 200, 200, 200]
--- response_body eval
["Pass", "Pass", "Pass", "Pass", "Pass", "Pass", "Pass", "Pass"]
=== TEST 2: Valid SSL upstream
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 200 'Pass';
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[200, 200, 200, 200, 200, 200, 200, 200]
--- response_body eval
["Pass", "Pass", "Pass", "Pass", "Pass", "Pass", "Pass", "Pass"]
=== TEST 3: Valid upstream no server
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[502, 502, 502, 502, 502, 502, 502, 502]
=== TEST 4: Valid SSL upstream no server
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[502, 502, 502, 502, 502, 502, 502, 502]
=== TEST 5: Round robin
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.2:8000;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:8000;
	return 202 'Pass 2';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 201, 202, 201, 202, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2"]
=== TEST 6: Round robin SSL
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 202 'Pass 2';
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 201, 202, 201, 202, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2"]
