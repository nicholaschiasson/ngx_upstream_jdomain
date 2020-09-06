FROM nicholaschiasson/nginx-module-toolbox:amazonlinux-2-1.18.0-1

WORKDIR /src/openssl
RUN curl -sL https://www.openssl.org/source/openssl-1.1.1g.tar.gz -o openssl.tar.gz
RUN tar --strip-components=1 -xzf openssl.tar.gz

WORKDIR /github/workspace
