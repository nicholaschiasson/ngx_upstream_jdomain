use Test::Nginx::Socket 'no_plan';

add_response_body_check(sub {
	my ($block, $body, $req_idx, $repeated_req_idx, $dry_run) = @_;
	if ($body eq "Interval") {
		`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
		sleep(4);
	} elsif ($body eq "Flip") {
		`echo 'local-data: "example.com 1 A 127.0.0.3"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
		sleep(1);
	} elsif ($body eq "Flop") {
		`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
		sleep(1);
	}
});

no_shuffle(); # NO SHUFFLE DUE TO must_die IN SOME TESTS!
run_tests();

__DATA__

=== TEST 1: Valid domain
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com;
}
server {
	listen 127.0.0.2:80;
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
=== TEST 1: IP in place of domain
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain 127.0.0.2;
}
server {
	listen 127.0.0.2:80;
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
=== TEST 1: IP with port in place of domain
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain 127.0.0.2:8000;
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
=== TEST 1: Missing domain
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain;
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request
GET /
--- must_die
--- error_log
invalid number of arguments in "jdomain" directive
--- no_check_leak
=== TEST 2: Valid port
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
=== TEST 2: Invalid port too low
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=0;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "port=0"
--- no_check_leak
=== TEST 2: Invalid port negative
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=-1;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "port=-1"
--- no_check_leak
=== TEST 2: Invalid port too high
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=99999999;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "port=99999999"
--- no_check_leak
=== TEST 2: Invalid port type
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=example;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "port=example"
--- no_check_leak
=== TEST 2: Invalid port empty
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "port="
--- no_check_leak
=== TEST 3: Valid max_ips
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=1;
}
server {
	listen 127.0.0.2;
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
=== TEST 3: Invalid max_ips too low
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=0;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "max_ips=0"
--- no_check_leak
=== TEST 3: Invalid max_ips negative
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=-1;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "max_ips=-1"
--- no_check_leak
=== TEST 3: Invalid max_ips too high
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=12345678901234567890;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "max_ips=12345678901234567890"
--- no_check_leak
=== TEST 3: Invalid max_ips type
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=example;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "max_ips=example"
--- no_check_leak
=== TEST 3: Invalid max_ips empty
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com max_ips=;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "max_ips="
--- no_check_leak
=== TEST 4: Valid interval
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=1;
}
server {
	listen 127.0.0.2:80;
	return 201 'Interval';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 201, 202, 202, 202, 202, 202, 202]
--- response_body eval
["Interval", "Interval", "Pass", "Pass", "Pass", "Pass", "Pass", "Pass"]
=== TEST 4: Valid interval alternative
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf && unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=6;
}
server {
	listen 127.0.0.2:80;
	return 201 'Interval';
}
server {
	listen 127.0.0.3:80;
	return 202 'Pass';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /"]
--- error_code eval
[201, 201, 201, 202, 202, 202, 202, 202]
--- response_body eval
["Interval", "Interval", "Interval", "Pass", "Pass", "Pass", "Pass", "Pass"]
=== TEST 4: Invalid interval zero
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=0;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "interval=0"
--- no_check_leak
=== TEST 4: Invalid interval negative
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=-1;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "interval=-1"
--- no_check_leak
=== TEST 4: Invalid interval too high
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=12345678901234567890;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "interval=12345678901234567890"
--- no_check_leak
=== TEST 4: Invalid interval type
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=example;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "interval=example"
--- no_check_leak
=== TEST 4: Invalid interval empty
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com interval=;
}
--- config
--- request
GET /
--- must_die
--- error_log
invalid parameter "interval="
--- no_check_leak
