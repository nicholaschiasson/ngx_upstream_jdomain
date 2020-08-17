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
- **[rust](https://www.rust-lang.org/tools/install)**: dependency of
	`cargo-make`.
- **[cargo-make](https://sagiegurari.github.io/cargo-make/#installation)**: to
	run common development tasks such as building, testing, and formatting code.

### Task Runner

`cargo-make` is an advanced task runner that will enabled you to easily perform
common development operations like formatting the code, building the module,
running the test suite, and running code analysis. You can see the task
definitions in the file `Makefile.toml`. Installing `cargo-make` will result in
a standalone executable called `makers` as well as a `cargo` extension which
can be executed via `cargo make`. As this project is not a `rust` crate, it is
recommended to simply use `makers`.

Also note that for simplicity's sake, the task runner uses docker to run all
tasks. This means the build binary is not targetting your host platform.

#### Default Task

To add value, the default task (ie. simply running `makers` alone) will begin
an interactive bash session inside the docker container used for this project.

This should help with debugging and general workflow.

#### Formatting

Incorrectly formatted code will cause the github actions linting job to fail.
To avoid this, you can run the format task before pushing new changes, like so:

```bash
makers format
```

This formatting is performed by a tool called `clang-format`. You can find the
config options for this defined in the file `./.clang-format`.

#### Building

You can build nginx with the module by running the build task, like so:

```bash
makers build
```

This will output a `./bin/` directory, which will contain the nginx source for
the version of nginx defined in the file `./.env` as well as an nginx binary at
`./bin/sbin/nginx`. You add the directories in `./bin/workdir/src/` to your
editor's includes path so facilitate local development.

#### Static Code Analysis

You can run a static analysis on the code via the analyse task:

```bash
makers analyse
```

This analysis is performed by a tool called `clang-tidy`. You can find the
config options for this defined in the file `./.clang-tidy`.

#### Testing

You can run the test suite using the test task, like so:

```bash
makers test
```

### Running GitHub Actions

With `act`, you can simulate the workflow that will run on GitHub servers once
you push changes.

There is more than one job in the main workflow, so you need to specify the
test job when you run `act`. For example, you can use this command to run the
code format validation:

```shell
act -vj lint
```

Note that the `lint` job does not format your code, it only checks that the
formatting is as expected.

Also note that `-v` is used to enable verbose mode to give more visibility on
everything `act` is doing.

The jobs you can (and should) run locally are `lint`, `build`, `analyse`, and
`test`. The `test` job depends on the output from the `build` job. To keep the
output from the build job, you can add the `-b` flag to `act`, or you may
simply use the task runner to build.

### Known Issues

At the moment? None! 🎉

If you discover a bug or have a question to raise, please
[open an issue](https://github.com/nicholaschiasson/ngx_upstream_jdomain/issues/new/choose).

## Original Author

wdaike <wdaike@163.com> (https://github.com/wdaike), Baidu Inc.
