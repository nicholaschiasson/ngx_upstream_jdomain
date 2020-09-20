/*
 * this module (C) wudaike
 * this module (C) Baidu, Inc.
 */

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_JDOMAIN_STATUS_DONE 0
#define NGX_JDOMAIN_STATUS_WAIT 1

typedef struct
{
	struct
	{
		ngx_str_t domain;
		time_t interval;
		ngx_uint_t max_ips;
		in_port_t port;
		ngx_uint_t retry;
		ngx_uint_t strict;
	} conf;

	struct
	{
		struct
		{
			ngx_array_t* addrs;
			ngx_array_t* names;
			ngx_http_upstream_server_t* server;
			ngx_array_t* sockaddrs;
		} data;

		struct
		{
			time_t access;
			ngx_int_t status;
		} resolve;
	} state;

	ngx_http_upstream_srv_conf_t* parent;
} ngx_http_upstream_jdomain_srv_conf_t;

static ngx_int_t
ngx_http_upstream_init_jdomain(ngx_conf_t* cf, ngx_http_upstream_srv_conf_t* us);

static ngx_int_t
ngx_http_upstream_init_jdomain_peer(ngx_http_request_t* r, ngx_http_upstream_srv_conf_t* us);

static void
ngx_http_upstream_jdomain_resolve_handler(ngx_resolver_ctx_t* ctx);

static void*
ngx_http_upstream_jdomain_create_conf(ngx_conf_t* cf);

static char*
ngx_http_upstream_jdomain(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);

static ngx_command_t ngx_http_upstream_jdomain_commands[] = {
	{ ngx_string("jdomain"), NGX_HTTP_UPS_CONF | NGX_CONF_1MORE, ngx_http_upstream_jdomain, 0, 0, NULL },
	ngx_null_command
};

static ngx_http_module_t ngx_http_upstream_jdomain_module_ctx = {
	NULL,                                  /* preconfiguration */
	NULL,                                  /* postconfiguration */
	NULL,                                  /* create main configuration */
	NULL,                                  /* init main configuration */
	ngx_http_upstream_jdomain_create_conf, /* create server configuration */
	NULL,                                  /* merge server configuration */
	NULL,                                  /* create location configuration */
	NULL                                   /* merge location configuration */
};

ngx_module_t ngx_http_upstream_jdomain_module = { NGX_MODULE_V1,
	                                                &ngx_http_upstream_jdomain_module_ctx, /* module context */
	                                                ngx_http_upstream_jdomain_commands,    /* module directives */
	                                                NGX_HTTP_MODULE,                       /* module type */
	                                                NULL,                                  /* init master */
	                                                NULL,                                  /* init module */
	                                                NULL,                                  /* init process */
	                                                NULL,                                  /* init thread */
	                                                NULL,                                  /* exit thread */
	                                                NULL,                                  /* exit process */
	                                                NULL,                                  /* exit master */
	                                                NGX_MODULE_V1_PADDING };

static ngx_int_t
ngx_http_upstream_init_jdomain(ngx_conf_t* cf, ngx_http_upstream_srv_conf_t* us)
{
	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "ngx_http_upstream_jdomain_module: init jdomain");

	if (ngx_http_upstream_init_round_robin(cf, us) != NGX_OK) {
		return NGX_ERROR;
	}

	us->peer.init = ngx_http_upstream_init_jdomain_peer;

	return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_init_jdomain_peer(ngx_http_request_t* r, ngx_http_upstream_srv_conf_t* us)
{
	ngx_http_upstream_jdomain_srv_conf_t* jcf;
	ngx_http_core_loc_conf_t* clcf;
	ngx_resolver_ctx_t* ctx;
	ngx_int_t rc;

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: init jdomain peer");

	rc = ngx_http_upstream_init_round_robin_peer(r, us);
	if (rc != NGX_OK) {
		goto end;
	}

	jcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_jdomain_module);
	clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);

	if (!jcf->conf.retry) {
		r->upstream->peer.tries = 1;
	}

	if (jcf->state.resolve.status == NGX_JDOMAIN_STATUS_WAIT || ngx_time() <= jcf->state.resolve.access + jcf->conf.interval) {
		goto end;
	}

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: update from DNS cache");

	ctx = ngx_resolve_start(clcf->resolver, NULL);
	if (ctx == NULL) {
		ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_start fail");
		rc = NGX_ERROR;
		goto end;
	}

	if (ctx == NGX_NO_RESOLVER) {
		ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: no resolver");
		rc = NGX_ERROR;
		goto end;
	}

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_start ok");

	ctx->name = jcf->conf.domain;
	ctx->handler = ngx_http_upstream_jdomain_resolve_handler;
	ctx->data = jcf;
	ctx->timeout = clcf->resolver_timeout;

	jcf->state.resolve.status = NGX_JDOMAIN_STATUS_WAIT;
	rc = ngx_resolve_name(ctx);
	if (rc != NGX_OK) {
		ngx_log_error(
		  NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_name \"%V\" fail", &ctx->name);
	}

end:
	return rc;
}

static void
ngx_http_upstream_jdomain_resolve_handler(ngx_resolver_ctx_t* ctx)
{
	ngx_http_upstream_jdomain_srv_conf_t* jcf;
	ngx_uint_t i;
	struct sockaddr* sockaddr;
	u_char* name;
	ngx_addr_t* addr;

	jcf = (ngx_http_upstream_jdomain_srv_conf_t*)ctx->data;

	ngx_log_debug3(NGX_LOG_DEBUG_CORE,
	               ctx->resolver->log,
	               0,
	               "ngx_http_upstream_jdomain_module: \"%V\" resolved state (%i: %s)",
	               &ctx->name,
	               ctx->state,
	               ngx_resolver_strerror(ctx->state));

	if (ctx->state || ctx->naddrs == 0) {
		jcf->state.data.server->down = jcf->parent->servers->nelts > 1 && (jcf->conf.strict || ctx->state == NGX_RESOLVE_FORMERR ||
		                                                                   ctx->state == NGX_RESOLVE_NXDOMAIN);
		ngx_log_error((jcf->state.data.server->down ? NGX_LOG_WARN : NGX_LOG_ERR),
		              ctx->resolver->log,
		              0,
		              "ngx_http_upstream_jdomain_module: resolver failed, \"%V\" (%i: %s)",
		              &ctx->name,
		              ctx->state,
		              ngx_resolver_strerror(ctx->state));
		if (!jcf->state.data.server->down) {
			goto end;
		}
	}

	jcf->state.data.sockaddrs->nelts = 0;
	jcf->state.data.addrs->nelts = 0;
	jcf->state.data.server->naddrs = 0;
	for (i = 0; i < ngx_min(ctx->naddrs, jcf->conf.max_ips); i++) {
		sockaddr = ngx_array_push(jcf->state.data.sockaddrs);
		if (!sockaddr) {
			ngx_log_error(NGX_LOG_ERR, ctx->resolver->log, 0, "ngx_http_upstream_jdomain_module: ngx_array_push sockaddr fail");
			goto end;
		}
		ngx_memzero(sockaddr, NGX_SOCKADDRLEN);
		name = ngx_array_push(jcf->state.data.names);
		if (!name) {
			ngx_log_error(NGX_LOG_ERR, ctx->resolver->log, 0, "ngx_http_upstream_jdomain_module: ngx_array_push name fail");
			goto end;
		}
		ngx_memzero(name, NGX_SOCKADDR_STRLEN);
		addr = ngx_array_push(jcf->state.data.addrs);
		if (!addr) {
			ngx_log_error(NGX_LOG_ERR, ctx->resolver->log, 0, "ngx_http_upstream_jdomain_module: ngx_array_push addr fail");
			goto end;
		}
		ngx_memzero(addr, sizeof(ngx_addr_t));
		ngx_memcpy(sockaddr, ctx->addrs[i].sockaddr, ctx->addrs[i].socklen);
		ngx_inet_set_port(sockaddr, jcf->conf.port);
		ngx_memcpy(name, ctx->addrs[i].name.data, ctx->addrs[i].name.len);
		addr->sockaddr = sockaddr;
		addr->socklen = ctx->addrs[i].socklen;
		addr->name.data = name;
		addr->name.len = ctx->addrs[i].name.len;
		jcf->state.data.server->down = 0;
		jcf->state.data.server->naddrs++;
	}

end:
	ngx_resolve_name_done(ctx);

	jcf->state.resolve.access = ngx_time();
	jcf->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;
}

static void*
ngx_http_upstream_jdomain_create_conf(ngx_conf_t* cf)
{
	ngx_http_upstream_jdomain_srv_conf_t* conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_jdomain_srv_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->conf.interval = 1;
	conf->conf.max_ips = 20;
	conf->conf.port = 80;
	conf->conf.retry = 1;

	conf->state.data.addrs = ngx_array_create(cf->pool, 1, sizeof(ngx_addr_t));
	if (!conf->state.data.addrs) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 1, "ngx_http_upstream_jdomain_module: ngx_array_create addrs fail");
		return NULL;
	}

	conf->state.data.names = ngx_array_create(cf->pool, 1, NGX_SOCKADDR_STRLEN);
	if (!conf->state.data.names) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 1, "ngx_http_upstream_jdomain_module: ngx_array_create names fail");
		return NULL;
	}

	conf->state.data.sockaddrs = ngx_array_create(cf->pool, 1, NGX_SOCKADDRLEN);
	if (!conf->state.data.sockaddrs) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 1, "ngx_http_upstream_jdomain_module: ngx_array_create sockaddrs fail");
		return NULL;
	}

	conf->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;

	return conf;
}

static char*
ngx_http_upstream_jdomain(ngx_conf_t* cf, ngx_command_t* cmd, void* conf)
{
	ngx_http_upstream_srv_conf_t* uscf;
	ngx_http_upstream_jdomain_srv_conf_t* jcf;

	struct sockaddr* sockaddr;
	u_char* name;
	ngx_addr_t* addr;

	ngx_pool_t* pool;
	ngx_str_t *value, s;
	ngx_int_t port;
	ngx_url_t u;
	ngx_uint_t i;
	u_char* errstr;
	char* rc;

	uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

	uscf->peer.init_upstream = ngx_http_upstream_init_jdomain;
	uscf->flags = NGX_HTTP_UPSTREAM_CREATE | NGX_HTTP_UPSTREAM_WEIGHT | NGX_HTTP_UPSTREAM_MAX_FAILS |
	              NGX_HTTP_UPSTREAM_FAIL_TIMEOUT | NGX_HTTP_UPSTREAM_DOWN | NGX_HTTP_UPSTREAM_BACKUP |
	              NGX_HTTP_UPSTREAM_MAX_CONNS;

	jcf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstream_jdomain_module);

	errstr = ngx_pcalloc(cf->pool, NGX_MAX_CONF_ERRSTR);
	if (!errstr) {
		rc = "ngx_http_upstream_jdomain_module: ngx_pcalloc errstr fail";
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, rc);
		return rc;
	}

	pool = ngx_create_pool(NGX_MIN_POOL_SIZE, cf->log);
	if (!pool) {
		rc = "ngx_http_upstream_jdomain_module: ngx_create_pool pool fail";
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, rc);
		return rc;
	}

	rc = NGX_CONF_OK;

	value = cf->args->elts;

	for (i = 2; i < cf->args->nelts; i++) {
		if (ngx_strncmp(value[i].data, "port=", 5) == 0) {
			port = ngx_atoi(value[i].data + 5, value[i].len - 5);
			if (port < 1 || port != (in_port_t)port) {
				goto invalid;
			}
			jcf->conf.port = port;
			continue;
		}

		if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
			s.len = value[i].len - 9;
			s.data = &value[i].data[9];
			jcf->conf.interval = ngx_parse_time(&s, 1);
			if (jcf->conf.interval == (time_t)NGX_ERROR) {
				goto invalid;
			}
			continue;
		}

		if (ngx_strncmp(value[i].data, "max_ips=", 8) == 0) {
			jcf->conf.max_ips = ngx_atoi(value[i].data + 8, value[i].len - 8);
			if (jcf->conf.max_ips < 1) {
				goto invalid;
			}
			continue;
		}

		if (value[i].len == 9 && ngx_strncmp(value[i].data, "retry_off", 9) == 0) {
			jcf->conf.retry = 0;
			continue;
		}

		if (value[i].len == 6 && ngx_strncmp(value[i].data, "strict", 6) == 0) {
			jcf->conf.strict = 1;
			continue;
		}

		goto invalid;
	}

	jcf->conf.domain.data = value[1].data;
	jcf->conf.domain.len = value[1].len;

	ngx_memzero(&u, sizeof(ngx_url_t));
	u.url = value[1];
	u.default_port = jcf->conf.port;

	if (!uscf->servers) {
		uscf->servers = ngx_array_create(cf->pool, 1, sizeof(ngx_http_upstream_server_t));
		if (!uscf->servers) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create servers fail");
			goto failure;
		}
	}

	jcf->state.data.server = ngx_array_push(uscf->servers);
	if (!jcf->state.data.server) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push server fail");
		goto failure;
	}
	ngx_memzero(jcf->state.data.server, sizeof(ngx_http_upstream_server_t));
	jcf->state.data.server->addrs = jcf->state.data.addrs->elts;
	jcf->state.data.server->name = jcf->conf.domain;

	if (ngx_parse_url(pool, &u) != NGX_OK) {
		if (uscf->servers->nelts < 2) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: %s in upstream \"%V\"", u.err ? u.err : "error", &u.url);
			goto failure;
		}
		ngx_conf_log_error(
		  NGX_LOG_WARN, cf, 0, "ngx_http_upstream_jdomain_module: %s in upstream \"%V\"", u.err ? u.err : "error", &u.url);
		jcf->state.data.server->down = 1;
	}

	for (i = 0; i < ngx_min(u.naddrs, jcf->conf.max_ips); i++) {
		sockaddr = ngx_array_push(jcf->state.data.sockaddrs);
		if (!sockaddr) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push sockaddr fail");
			goto failure;
		}
		ngx_memzero(sockaddr, NGX_SOCKADDRLEN);
		name = ngx_array_push(jcf->state.data.names);
		if (!name) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push name fail");
			goto failure;
		}
		ngx_memzero(name, NGX_SOCKADDR_STRLEN);
		addr = ngx_array_push(jcf->state.data.addrs);
		if (!addr) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push addr fail");
			goto failure;
		}
		ngx_memzero(addr, sizeof(ngx_addr_t));
		ngx_memcpy(sockaddr, u.addrs[i].sockaddr, u.addrs[i].socklen);
		ngx_memcpy(name, u.addrs[i].name.data, u.addrs[i].name.len);
		addr->sockaddr = sockaddr;
		addr->socklen = u.addrs[i].socklen;
		addr->name.data = name;
		addr->name.len = u.addrs[i].name.len;
		jcf->state.data.server->naddrs++;
	}
	jcf->state.resolve.access = ngx_time();

	goto end;

invalid:
	ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: invalid parameter \"%V\"", &value[i]);

failure:
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, (const char*)errstr);
	rc = (char*)errstr;

end:
	ngx_destroy_pool(pool);
	return rc;
}
