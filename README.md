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

	./configure --add-module=/path/to/this/directory
	make
	make install

## Usage

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

## Synopsis

	Syntax: jdomain <domain-name> [port=80] [max_ips=20] [interval=1] [retry_off] [fallback= [strict]]
	Context:    upstream
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

See https://www.nginx.com/resources/wiki/modules/domain_resolve/ for details.

## Development

### Prerequisites

> TODO

### Building

> TODO

### Testing

> TODO

## Original Author

wdaike <wdaike@163.com>, Baidu Inc.
