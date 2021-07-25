use Test::Nginx::Socket 'no_plan';

workers(2);
run_tests();

__DATA__

=== TEST 1: Load balancing with server
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 801, 802, 801, 802, 801, 802]
--- response_body eval
["Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2", "Pass 1", "Pass 2"]
=== TEST 2: Load balancing with server and multiple addresses
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 801, 802, 803, 801, 802]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 3: Load balancing with server and least_conn
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
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
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 801, 802, 803, 801, 802]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 4: Load balancing with server and keepalive
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
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
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 801, 802, 803, 801, 802]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 5: Load balancing with server and hash
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
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
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 801, 801, 801, 801, 801, 801, 801]
--- response_body eval
["Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1", "Pass 1"]
=== TEST 6: Load balancing with server and zone
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
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
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 801, 802, 803, 801, 802]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2", "Pass 3", "Pass 1", "Pass 2"]
=== TEST 7: Load balancing with server and multiple jdomain
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.ca 1 A 127.0.0.5"' >> /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.ca 1 A 127.0.0.6"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com interval=30;
	jdomain example.ca interval=30;
}
server {
	listen 127.0.0.2:80;
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
server {
	listen 127.0.0.5:80;
	return 804 'Pass 4';
}
server {
	listen 127.0.0.6:80;
	return 805 'Pass 5';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 804, 805, 801, 802, 803]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 4", "Pass 5", "Pass 1", "Pass 2", "Pass 3"]
=== TEST 8: Load balancing with server and multiple jdomain for the same domain
--- init
`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.2;
	jdomain example.com interval=30;
	jdomain example.com interval=30 port=8080;
}
server {
	listen 127.0.0.2:80;
	return 801 'Pass 1';
}
server {
	listen 127.0.0.3:80;
	return 802 'Pass 2';
}
server {
	listen 127.0.0.4:80;
	return 803 'Pass 3';
}
server {
	listen 127.0.0.3:8080;
	return 804 'Pass 4';
}
server {
	listen 127.0.0.4:8080;
	return 805 'Pass 5';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[801, 802, 803, 804, 805, 801, 802, 803]
--- response_body eval
["Pass 1", "Pass 2", "Pass 3", "Pass 4", "Pass 5", "Pass 1", "Pass 2", "Pass 3"]
