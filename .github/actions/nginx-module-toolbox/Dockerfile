ARG IMAGE_BASE
ARG NGINX_VERSION

FROM nicholaschiasson/nginx-module-toolbox:${IMAGE_BASE}-${NGINX_VERSION}-2.13.0

WORKDIR /src/openssl
RUN curl -sL https://www.openssl.org/source/openssl-1.1.1n.tar.gz -o openssl.tar.gz
RUN tar --strip-components=1 -xzf openssl.tar.gz

WORKDIR /src/modules

RUN curl -sL https://github.com/yaoweibin/nginx_upstream_check_module/archive/master.tar.gz -o nginx_upstream_check_module.tar.gz
RUN mkdir nginx_upstream_check_module
RUN tar --directory=nginx_upstream_check_module --strip-components=1 -xzf nginx_upstream_check_module.tar.gz

RUN curl -sL https://github.com/vozlt/nginx-module-vts/archive/v0.1.18.tar.gz -o nginx-module-vts.tar.gz
RUN mkdir nginx-module-vts
RUN tar --directory=nginx-module-vts --strip-components=1 -xzf nginx-module-vts.tar.gz

RUN rm *.tar.gz

WORKDIR /github/workspace
