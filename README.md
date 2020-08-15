# ngx_upstream_jdomain

An asynchronous domain name resolution module for nginx upstream.

This module allows you to use a domain name in an upstream block and expect the
domain name to be dynamically resolved so your upstream may be resilient to DNS
entry updates.

The module does not perform DNS resolution automatically on some interval.
Instead, the DNS resolution needs to be prompted by a request for the given
upstream. If nginx serves a connection bound for a jdomain upstream, and the
configured `interval` has elapsed, then the module will perform a DNS lookup.

**Important Note**: Due to the asynchronous nature of this module and the fact
that its DNS resolution is triggered by incoming requests, the request that
prompts a lookup will actually still be forwarded to the upstream that was
resolved and cached before the DNS lookup happens. Depending on the scenario,
this could result in a one off failure when changing the states of
upstreams. This is important to keep in mind to ensure graceful transitions of
your upstreams.

This repository is a fork of [a repository](https://github.com/wdaike/ngx_upstream_jdomain)
originally authored by [wdaike](https://github.com/wdaike). As that project is
no longer maintained, this repository aims to be its successor and is now
several features ahead.

## Installation

[Build nginx](http://nginx.org/en/docs/configure.html) with this repository as
a static module.

```shell
./configure --add-module=/path/to/this/directory
make
make install
```

## Usage

```nginx
resolver 8.8.8.8; # Your Local DNS Server

# Basic upstream using domain name defaulting to port 80.
upstream backend_01 {
	jdomain example.com;
}

# Basic upstream specifying different port.
upstream backend_02 {
	jdomain example.com port=8080;
}

# Upstream with a fallback IP address to use in case of host not found or
# format errors on DNS resolution.
# Fallback defaults to same port used for the domain (which defaults to 80).
upstream backend_03 {
	jdomain example.com fallback=127.0.0.2;
}

# Upstream with fallback IP defaulting to port specified by port attribute.
upstream backend_04  {
	jdomain example.com port=8080 fallback=127.0.0.2;
}

# Upstream with fallback IP specifying its own port to use.
upstream backend_05 {
	jdomain example.com fallback=127.0.0.2:8080;
}

# Upstream which will use fallback for any and all DNS resolution errors.
upstream backend_06 {
	jdomain example.come fallback=127.0.0.2 strict;
}

server {
	listen 127.0.0.2:80;
	return 502 'An error.';
}

server {
	listen 127.0.0.2:8080;
	return 502 'A different error.';
}
```

## Synopsis

```
Syntax: jdomain <domain-name> [port=80] [max_ips=20] [interval=1] [retry_off] [fallback= [strict]]
Context: upstream
Attributes:
	port:       Backend's listening port.                                      (Default: 80)
	max_ips:    IP buffer size. Maximum number of resolved IPs to cache.       (Default: 20)
	interval:   How many seconds to resolve domain name.                       (Default: 1)
	retry_off:  Do not retry if one IP fails.
	fallback:   Optional IP and port to use if <domain-name> resolves no IPs,
	            resolves with a host not found error, or a format error.
	strict:     Forces use of fallback even in case of other resolution
	            errors, such as timeouts, DNS server failures, connection
	            refusals, etc.
```

See https://www.nginx.com/resources/wiki/modules/domain_resolve/ for details.

## Development

### Prerequisites

To facilitate local development and enable you to build and test the module,
you'll need some tools.

- **[Docker](https://docs.docker.com/get-docker/)**: to provide an environment
	to easily reproduce ideal conditions for building and testing.
- **[act](https://github.com/nektos/act#installation)**: to simulate executing
	github actions workflows locally to save you from pushing commits just to
	watch the CI fail.

### Linting, Static Analysis, Building, and Testing

The `./scripts/` directory contains a set of scripts used by the github actions
workflow. The aforementioned scripts make assumptions about their environment,
and thus will not work being executed directly. In order to use the scripts, you
may choose to use the Dockerfile at `./.github/actions/nginx-module-toolbox/Dockerfile`
directly and mount the project root, or you may simply make use of `act`.

The file `./scripts/lint.sh` exists for the purpose of validating code style.
Should your code fail the validation, you can automatically fix it by using the
program `clang-format` with the `-i` flag to format files in place. The config
options for this can be found in the file `./.clang-format`.

The file `./scripts/build.sh` exists for the purpose of building a minimal
nginx binary with the module statically linked.

The file `./scripts/test.sh` exists for the purpose of running the test suite
against an nginx binary built with the module. This script depends on the
output of `./scripts/build.sh`.

The file `./scripts/analyse.sh` exists for the purpose of running static code
analysis on the module source using `clang-tidy`. This script depends on the
output of `./scripts/build.sh` as well. Technically it does not actually need a
binary, but the build script fetches and caches the nginx source, which this
script does depend on. The config options for `clang-tidy` can be found in the
file `./.clang-tidy`.

With `act`, you can run these scripts as they would be run when you push
changes to the remote and the workflow is triggered via github actions.

Since there is more than one job in the main workflow, you need to specify the
test job when you run `act`. This command builds nginx and runs the tests, you
can expect to be using it a lot:

```shell
act -vj test
```

Note `-v` is used to enable verbose mode to give more visibility on everything
`act` is doing.

Also note that some jobs deal with uploading and downloading artifacts, so they
may persist across jobs in the github actions workflow. These actions will fail
locally using `act`, so it's best to comment them out if you for example intend
to run the build, analyse, and test jobs.

> TODO: Improve the user friendliness of these operations. https://github.com/nicholaschiasson/ngx_upstream_jdomain/issues/17

### Dumb Tips & Known Issues

#### Hold Onto That Binary

In some cases, you can find yourself struggling with fixing tests and running
them many times over and over again without needing to make further changes to
the C code. The fact that the `Test` job builds nginx each time also makes
things a little more bothersome.

To alleviate this extra suffering, what I do is to:

1. First run `act` once with an extra flag `-b` to bind the project directory
	as amounted volume. You will then have a git ignored `./bin/` directory at
	the project root.
2. Then comment out the `Build` step inside `./.github/workflows/main.yml` and
	and continue to run `act` as usual to reuse the nginx binary you had
	previously built.

This is obviously not really a great long term solution.

[This issue](https://github.com/nicholaschiasson/ngx_upstream_jdomain/issues/17)
exists to track improvements to local development tooling. Ideally, there will
soon be a setup to run jobs in isolation reliably and simplistically.

#### The Damn Tests Sometimes Hang...

This is still sort of mysterious... From time to time, the tests simply don't
run. It's unclear what the real cause is, so more investigation is required,
but there's strong indication that it's something to do with perl, prove, or
the Test::Nginx framework.

[This issue](https://github.com/nicholaschiasson/ngx_upstream_jdomain/issues/26)
exists to track the investigation and solution of this bug.

## Original Author

wdaike <wdaike@163.com> (https://github.com/wdaike), Baidu Inc.
