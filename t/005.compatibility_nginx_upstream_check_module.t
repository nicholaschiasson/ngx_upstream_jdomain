use Test::Nginx::Socket 'no_plan';

add_response_body_check(sub {
	sleep(4);
});

master_on();

run_tests();

__DATA__

=== TEST 1: Upstream health check
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.ca 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
resolver 127.0.0.88;
upstream upstream_test {
	server 127.0.0.6:8080;
	jdomain example.com port=8000;
	jdomain example.ca port=8000;
	check interval=1000 rise=1 fall=1 timeout=1000 type=http default_down=false;
	check_http_send "GET / HTTP/1.0\r\n\r\n";
	check_http_expect_alive http_2xx;
}
server {
	listen 127.0.0.3:8000;
	return 200 'Pass';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
location = /status {
	check_status;
}
--- request eval
["GET /", "GET /", "GET /status?format=csv"]
--- error_code eval
[200, 200, 200]
--- response_body_like eval
["Pass", "Pass",
"0,upstream_test,127.0.0.6:8080,down,0,[0-9]+,http,0
1,upstream_test,127.0.0.2:8000,down,0,[0-9]+,http,0
2,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
3,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
4,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
5,upstream_test,127.0.0.3:8000,up,[0-9]+,0,http,0
6,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
7,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
8,upstream_test,NGX_UPSTREAM_JDOMAIN_BUFFER,down,0,[0-9]+,http,0
"]
--- no_check_leak
