use Test::Nginx::Socket 'no_plan';

workers(2);
run_tests();

__DATA__

=== TEST 1: Load balancing with server
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
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
=== TEST 2: Load balancing with server and multiple addresses
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 203 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 203, 201, 202, 203, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 3: Load balancing with server and least_conn
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	least_conn;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 203 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 203, 201, 202, 203, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 4: Load balancing with server and keepalive
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com;
	keepalive 1;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 203 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 203, 201, 202, 203, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 5: Load balancing with server and hash
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	hash ${remote_addr};
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 203 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 201, 201, 201, 201, 201, 201, 201]
--- response_body eval
["Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1"]
=== TEST 6: Load balancing with server and zone
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	zone example 32k;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 203 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 202, 203, 201, 202, 203, 201, 202]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
