use Test::Nginx::Socket 'no_plan';

add_response_body_check(sub {
	my ($block, $body, $req_idx, $repeated_req_idx, $dry_run) = @_;
	if ($body eq "Pass") {
		`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
	} elsif ($body eq "Pass 1") {
		`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
	} elsif ($body eq "Kill") {
		`echo 'local-zone: "example.com" always_refuse' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
	} else {
		`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
	}
	sleep(2);
});

run_tests();

__DATA__

=== TEST 1: Dynamic upstream
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
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
[201, 201, 202, 202, 201, 201, 202, 202]
--- response_body eval
["Pass 1", "Pass 1", "Pass 2", "Pass 2", "Pass 1", "Pass 1", "Pass 2", "Pass 2"]
=== TEST 2: Dynamic SSL upstream
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
[201, 201, 202, 202, 201, 201, 202, 202]
--- response_body eval
["Pass 1", "Pass 1", "Pass 2", "Pass 2", "Pass 1", "Pass 1", "Pass 2", "Pass 2"]
=== TEST 3: Dynamic upstream with periodically failing DNS resolution
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
=== TEST 4: Dynamic SSL upstream with periodically failing DNS resolution
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
=== TEST 5: Dynamic upstream with periodically failing DNS resolution and backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.3:8000;
	return 999 'Backup';
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
[200, 200, 999, 999, 200, 200, 999, 999]
--- response_body eval
["Pass", "Pass", "Backup", "Backup", "Pass", "Pass", "Backup", "Backup"]
=== TEST 6: Dynamic SSL upstream with periodically failing DNS resolution and backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.3:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 999 'Backup';
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
[200, 200, 999, 999, 200, 200, 999, 999]
--- response_body eval
["Pass", "Pass", "Backup", "Backup", "Pass", "Pass", "Backup", "Backup"]
=== TEST 7: Dynamic upstream with constantly refused DNS resolution with non-strict backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.2:8000;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
=== TEST 8: Dynamic SSL upstream with constantly refused DNS resolution with non-strict backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
=== TEST 9: Dynamic upstream with constantly refused DNS resolution with strict backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.3:8000;
	return 999 'Backup';
}
server {
	listen 127.0.0.2:8000;
	return 200 'Kill';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[200, 200, 999, 999, 200, 200, 999, 999]
--- response_body eval
["Kill", "Kill", "Backup", "Backup", "Kill", "Kill", "Backup", "Backup"]
=== TEST 10: Dynamic SSL upstream with constantly refused DNS resolution with strict backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup;
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.3:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 999 'Backup';
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 200 'Kill';
}
--- config
location = / {
	proxy_pass https://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[200, 200, 999, 999, 200, 200, 999, 999]
--- response_body eval
["Kill", "Kill", "Backup", "Backup", "Kill", "Kill", "Backup", "Backup"]
=== TEST 11: Dynamic upstream with constantly refused DNS resolution with strict down backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup down;
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.2:8000;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
=== TEST 12: Dynamic SSL upstream with constantly refused DNS resolution with strict down backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:8000 backup down;
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
=== TEST 13: Dynamic upstream with constantly refused DNS resolution with strict but no backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.2:8000;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
=== TEST 14: Dynamic SSL upstream with constantly refused DNS resolution with strict but no backup
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 strict;
}
server {
	listen 127.0.0.2:8000 ssl;
	ssl_certificate /etc/ssl/nginx/test/cert.pem;
	ssl_certificate_key /etc/ssl/nginx/test/key.pem;
	return 200 'Kill';
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
["Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill", "Kill"]
