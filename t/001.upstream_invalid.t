use Test::Nginx::Socket 'no_plan';

no_shuffle(); # NO SHUFFLE DUE TO must_die IN TEST 4!
run_tests();

__DATA__

=== TEST 1: Invalid upstream with backup
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:12345 backup;
	jdomain example.com port=8000;
}
server {
	listen 127.0.0.3:12345;
	return 999 'Backup';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[999, 999, 999, 999, 999, 999, 999, 999]
--- response_body eval
["Backup", "Backup", "Backup", "Backup", "Backup", "Backup", "Backup", "Backup"]
=== TEST 2: Invalid SSL upstream with backup
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:12345 backup;
	jdomain example.com port=8000;
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
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[999, 999, 999, 999, 999, 999, 999, 999]
--- response_body eval
["Backup", "Backup", "Backup", "Backup", "Backup", "Backup", "Backup", "Backup"]
=== TEST 3: Invalid upstream with backup but no server
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:12345 backup;
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
=== TEST 4: Invalid SSL upstream with backup but no server
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:12345 backup;
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
=== TEST 5: Invalid upstream without backup
THIS TEST MUST NOT BE FIRST! THERE SEEMS TO BE A BUG WITH 'must_die' WHICH
CAUSES THE FRAMEWORK TO HANG IF THIS TEST GOES FIRST!
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000;
}
--- config
--- request
GET /
--- must_die
--- error_log
host not found in upstream "example.com"
--- no_check_leak
=== TEST 6: Invalid upstream with down backup
--- init
`echo > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.3:12345 backup down;
	jdomain example.com port=8000;
}
--- config
--- request
GET /
--- must_die
--- error_log
host not found in upstream "example.com"
--- no_check_leak
