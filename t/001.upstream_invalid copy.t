use Test::Nginx::Socket 'no_plan';
run_tests();

__DATA__

=== TEST 1:
--- config
location = / {
    echo "hello, world!";
}
--- request
GET /
--- response_body
hello, world!
--- error_code: 200
