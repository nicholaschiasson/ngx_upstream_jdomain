FROM amazonlinux:2

WORKDIR /opt/nginx-module-toolbox

RUN yum groupinstall -y "Development Tools"

RUN yum install -y \
	bind-utils \
	gzip \
	openssl-devel \
	pcre-devel \
	perl \
	perl-App-cpanminus \
	procps \
	tar \
	unbound \
	valgrind

RUN cpanm --force Spiffy
RUN cpanm Test::Base
RUN cpanm Test::Nginx

COPY unbound.conf /opt/unbound.conf

RUN echo "local-data: \"ngx_upstream_jdomain.test 1 A 127.0.0.1\"" > /opt/ngx_upstream_jdomain_unbound_active_test.conf

RUN unbound -c /opt/unbound.conf -vvv
