FROM amazonlinux:2

WORKDIR /opt/ngx_upstream_jdomain

RUN yum groupinstall -y "Development Tools"

RUN yum install -y \
	bind-utils \
	gzip \
	openssl-devel \
	pcre-devel \
	procps \
	tar \
	unbound \
	valgrind
