use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: Virtual host traffic status
--- init
`echo 'local-data: "example.com 1 A 127.0.0.2"' > /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.com 1 A 127.0.0.3"' >> /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.ca 1 A 127.0.0.4"' >> /etc/unbound/unbound_local_zone.conf &&
echo 'local-data: "example.ca 1 A 127.0.0.5"' >> /etc/unbound/unbound_local_zone.conf &&
unbound-control reload` or die $!;
--- http_config
vhost_traffic_status_zone;
resolver 127.0.0.88;
upstream upstream_test {
	jdomain example.com port=8000 interval=32;
	jdomain example.ca port=8000 interval=32;
}
server {
	listen 127.0.0.2:8000;
	return 201 'Pass 1';
}
server {
	listen 127.0.0.3:8000;
	return 202 'Pass 2';
}
server {
	listen 127.0.0.4:8000;
	return 401 'Pass 3';
}
server {
	listen 127.0.0.5:8000;
	return 402 'Pass 4';
}
--- config
location = / {
	proxy_pass http://upstream_test;
}
location /status {
	vhost_traffic_status_display;
}
--- request eval
["GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /", "GET /status"]
--- error_code eval
[201, 202, 401, 402, 201, 202, 401, 200]
--- response_body_like eval
["Pass 1", "Pass 2", "Pass 3", "Pass 4", "Pass 1", "Pass 2", "Pass 3",
'^.*"upstreamZones":\{"upstream_test":\[\{"server":"127.0.0.2:8000","requestCounter":2,"inBytes":\d+,"outBytes":\d+,"responses":\{"1xx":0,"2xx":2,"3xx":0,"4xx":0,"5xx":0\},.*?\{"server":"127.0.0.4:8000","requestCounter":2,"inBytes":\d+,"outBytes":\d+,"responses":\{"1xx":0,"2xx":0,"3xx":0,"4xx":2,"5xx":0\},.*$']
