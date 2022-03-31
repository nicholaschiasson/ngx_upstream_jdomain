# ngx_upstream_jdomain

An asynchronous domain name resolution module for nginx upstream.

This module allows you to use a domain name in an upstream block and expect the
domain name to be dynamically resolved so your upstream may be resilient to DNS
entry updates.

The module does not perform DNS resolution automatically on some interval.
Instead, the DNS resolution needs to be prompted by a request for the given
upstream. If nginx serves a connection bound for a jdomain upstream, and the
configured `interval` has elapsed, then the module will perform a DNS lookup.

The module is compatible with other `upstream` scope directives. This means you
may populate an `upstream` block with multiple `jdomain` directives, multiple
`server` directives, `keepalive`, load balancing directives, etc. Note that
unless another load balancing method is specified in the `upstream` block, this
module makes use of the default round robin load balancing algorithm built into
nginx core.

**Important Note**: Should an alternate load balancing algorithm be specified,
**it must come _before_ the jdomain directive in the upstream block!** If this
is not followed, nginx **_will_** crash during runtime! This is because many
other load balancing modules explicitly extend the built in round robin, and
thus end up clobbering the jdomain initialization handlers, since jdomain is
technically a load balancer module as well. While this may not be the case with
all load balancer modules, it's better to stay on the safe side and place
jdomain after.

**Important Note**: Due to the non blocking nature of this module and the fact
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
a static or dynamic module.

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

# Upstream with a backup server to use in case of host not found or format
# errors on DNS resolution.
upstream backend_03 {
	server 127.0.0.2 backup;
	jdomain example.com;
}

# Upstream which will use backup for any and all DNS resolution errors.
upstream backend_04 {
	server 127.0.0.2 backup;
	jdomain example.com strict;
}

server {
	listen 127.0.0.2:80;
	return 502 'An error.';
}
```

## Synopsis

```
Syntax: jdomain <domain-name> [port=80] [max_ips=4] [interval=1] [strict]
Context: upstream
Attributes:
	port:       Backend's listening port.                                      (Default: 80)
	max_ips:    IP buffer size. Maximum number of resolved IPs to cache.       (Default: 4)
	interval:   How many seconds to resolve domain name.                       (Default: 1)
	ipver:      Only addresses of family IPv4 or IPv6 will be used if defined  (Default: 0)
	strict:     Require the DNS resolution to succeed and return addresses,
	            otherwise marks the underlying server and peers as down and
	            forces use of other servers in the upstream block if there
	            are any present. A failed resolution can be a timeout, DNS
	            server failure, connection refusals, response with no
	            addresses, etc.
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

### Debugging

We can use `valgrind` and `gdb` on nginx from inside the container.

First open an interactive shell in the container with:

```bash
$ makers
```

We'll use that session to run `valgrind`:

```bash
$ valgrind --vgdb=full --vgdb-error=0 /github/workspace/bin/static/nginx -p/github/workspace/t/servroot -cconf/nginx.conf
==15== Memcheck, a memory error detector
==15== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==15== Using Valgrind-3.13.0 and LibVEX; rerun with -h for copyright info
==15== Command: /github/workspace/bin/static/nginx -p/github/workspace/t/servroot -cconf/nginx.conf
==15==
==15== (action at startup) vgdb me ...
==15==
==15== TO DEBUG THIS PROCESS USING GDB: start GDB like this
==15==   /path/to/gdb /github/workspace/bin/static/nginx
==15== and then give GDB the following command
==15==   target remote | /usr/lib64/valgrind/../../bin/vgdb --pid=15
==15== --pid is optional if only one valgrind process is running
==15==
```

Next, find the container identifier so we can open another session inside it:

```bash
$ docker ps
CONTAINER ID        IMAGE                                     COMMAND             CREATED             STATUS              PORTS                    NAMES
55fab1e069ba        act-github-actions-nginx-module-toolbox   "bash"              4 seconds ago       Up 3 seconds        0.0.0.0:1984->1984/tcp   serene_newton
```

Use either the name or ID to execute a bash session inside the container:

```bash
$ docker exec -it serene_newton bash
```

We'll use this session to start `gdb` and target the valgrind gdb server we started in the other session:

```bash
$ gdb /github/workspace/bin/static/nginx
GNU gdb (GDB) Red Hat Enterprise Linux 8.0.1-30.amzn2.0.3
Copyright (C) 2017 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "x86_64-redhat-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from /github/workspace/bin/static/nginx...done.
(gdb)
```

From the gdb prompt, target the valgrind process and begin debugging:

```bash
(gdb) target remote | /usr/lib64/valgrind/../../bin/vgdb --pid=15
Remote debugging using | /usr/lib64/valgrind/../../bin/vgdb --pid=15
relaying data between gdb and process 15
warning: remote target does not support file transfer, attempting to access files from local filesystem.
Reading symbols from /lib64/ld-linux-x86-64.so.2...(no debugging symbols found)...done.
0x0000000004000ef0 in _start () from /lib64/ld-linux-x86-64.so.2
Missing separate debuginfos, use: debuginfo-install glibc-2.26-35.amzn2.x86_64
(gdb)
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
