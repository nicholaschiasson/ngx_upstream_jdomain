use Test::Nginx::Socket 'no_plan';

run_tests();

__DATA__

=== TEST 1: TO DO
--- config
--- request
GET /
--- todo
1: write tests for compatibility with nginx directives like server, keepalive, least_conn, etc.
