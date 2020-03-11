FROM amazonlinux:2

WORKDIR /opt/nginx-module-toolbox

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
