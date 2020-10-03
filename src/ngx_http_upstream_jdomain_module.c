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
		ngx_uint_t strict;
	} conf;
	struct
	{
		struct
		{
			ngx_array_t *addrs;
			ngx_uint_t naddrs;
			ngx_array_t *names;
			ngx_array_t *peerps;
			ngx_http_upstream_server_t *server;
			ngx_array_t *sockaddrs;
		} data;
		struct
		{
			time_t access;
			ngx_int_t status;
		} resolve;
	} state;
	ngx_http_upstream_srv_conf_t *parent;
} ngx_http_upstream_jdomain_instance_t;

typedef struct
{
	struct
	{
		ngx_http_upstream_init_pt default_init;
		ngx_http_upstream_init_peer_pt default_init_peer;
	} handlers;
	ngx_array_t *instances;
} ngx_http_upstream_jdomain_srv_conf_t;

static ngx_int_t
ngx_http_upstream_init_jdomain(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us);

static ngx_int_t
ngx_http_upstream_init_jdomain_peer(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us);

static void
ngx_http_upstream_jdomain_resolve_handler(ngx_resolver_ctx_t *ctx);

static void *
ngx_http_upstream_jdomain_create_conf(ngx_conf_t *cf);

static void *
ngx_http_upstream_jdomain_create_instance(ngx_conf_t *cf, ngx_array_t *instance_array);

static char *
ngx_http_upstream_jdomain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_command_t ngx_http_upstream_jdomain_commands[] = {
	{ ngx_string("jdomain"), NGX_HTTP_UPS_CONF | NGX_CONF_1MORE, ngx_http_upstream_jdomain, NGX_HTTP_SRV_CONF_OFFSET, 0, NULL },
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

static struct sockaddr_in INVALID_ADDR_SOCKADDR_IN = { 0 };
static const ngx_addr_t INVALID_ADDR = { (struct sockaddr *)(&INVALID_ADDR_SOCKADDR_IN),
	                                       sizeof(struct sockaddr_in),
	                                       ngx_string("0.0.0.0:0") };

static ngx_int_t
ngx_http_upstream_init_jdomain(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
	ngx_http_upstream_jdomain_srv_conf_t *jcf;
	ngx_http_upstream_jdomain_instance_t *instance;
	ngx_http_upstream_rr_peer_t *peer, **peerp;
	ngx_http_upstream_rr_peers_t *peers;
	ngx_uint_t i, j;

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "ngx_http_upstream_jdomain_module: init jdomain");

	jcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_jdomain_module);

	if (jcf->handlers.default_init(cf, us) != NGX_OK) {
		return NGX_ERROR;
	}

	jcf->handlers.default_init_peer = us->peer.init;
	us->peer.init = ngx_http_upstream_init_jdomain_peer;

	instance = jcf->instances->elts;
	peers = us->peer.data;
	for (i = 0; i < jcf->instances->nelts; i++) {
		peerp = instance[i].state.data.peerps->elts;
		j = 0;

		for (peer = peers->peer; peer; peer = peer->next) {
			if (instance[i].state.data.server->name.len == peer->server.len &&
			    ngx_strncmp(instance[i].state.data.server->name.data, peer->server.data, peer->name.len) == 0) {
				peerp[j++] = peer;
			}
		}

		if (j != instance[i].conf.max_ips) {
			ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: num peerps does not match max_ips");
			return NGX_ERROR;
		}

		for (j = instance[i].state.data.naddrs; j < instance[i].conf.max_ips; j++) {
			peerp[j]->down = 1;
		}

		instance[i].state.data.server->naddrs = instance[i].state.data.naddrs;
	}

	return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_init_jdomain_peer(ngx_http_request_t *r, ngx_http_upstream_srv_conf_t *us)
{
	ngx_http_upstream_jdomain_srv_conf_t *jcf;
	ngx_http_upstream_jdomain_instance_t *instance;
	ngx_http_core_loc_conf_t *clcf;
	ngx_resolver_ctx_t *ctx;
	ngx_uint_t i;
	ngx_int_t rc;

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: init jdomain peer");

	jcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_jdomain_module);

	rc = jcf->handlers.default_init_peer(r, us);
	if (rc != NGX_OK) {
		goto end;
	}

	clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
	instance = jcf->instances->elts;
	for (i = 0; i < jcf->instances->nelts; i++) {
		if (instance[i].state.resolve.status == NGX_JDOMAIN_STATUS_WAIT ||
		    ngx_time() <= instance[i].state.resolve.access + instance[i].conf.interval) {
			continue;
		}

		ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: update from DNS cache");

		ctx = ngx_resolve_start(clcf->resolver, NULL);
		if (ctx == NULL) {
			ngx_log_error(NGX_LOG_ALERT,
			              r->connection->log,
			              0,
			              "ngx_http_upstream_jdomain_module: ngx_resolve_start \"%V\" fail",
			              &instance[i].conf.domain);
			continue;
		}

		if (ctx == NGX_NO_RESOLVER) {
			ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: no resolver");
			rc = NGX_ERROR;
			goto end;
		}

		ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_start ok");

		ctx->name = instance[i].conf.domain;
		ctx->handler = ngx_http_upstream_jdomain_resolve_handler;
		ctx->data = &instance[i];
		ctx->timeout = clcf->resolver_timeout;

		rc = ngx_resolve_name(ctx);
		if (rc != NGX_OK) {
			ngx_log_error(
			  NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_name \"%V\" fail", &ctx->name);
			goto end;
		}
		instance[i].state.resolve.status = NGX_JDOMAIN_STATUS_WAIT;
	}

end:
	return rc;
}

static void
ngx_http_upstream_jdomain_resolve_handler(ngx_resolver_ctx_t *ctx)
{
	ngx_http_upstream_jdomain_instance_t *instance;
	ngx_uint_t i;
	struct sockaddr *sockaddr;
	u_char *name;
	ngx_addr_t *addr;
	ngx_http_upstream_rr_peer_t **peerp;

	instance = (ngx_http_upstream_jdomain_instance_t *)ctx->data;

	if (ctx->state || ctx->naddrs == 0) {
		instance->state.data.server->down =
		  instance->parent->servers->nelts > 1 &&
		  (instance->conf.strict || ctx->state == NGX_RESOLVE_FORMERR || ctx->state == NGX_RESOLVE_NXDOMAIN);
		ngx_log_error((instance->state.data.server->down ? NGX_LOG_WARN : NGX_LOG_ERR),
		              ctx->resolver->log,
		              0,
		              "ngx_http_upstream_jdomain_module: resolver failed, \"%V\" (%i: %s)",
		              &ctx->name,
		              ctx->state,
		              ngx_resolver_strerror(ctx->state));
		if (!instance->state.data.server->down) {
			goto end;
		}
	}

	addr = instance->state.data.addrs->elts;
	name = instance->state.data.names->elts;
	peerp = instance->state.data.peerps->elts;
	sockaddr = instance->state.data.sockaddrs->elts;
	instance->state.data.server->naddrs = 0;
	instance->state.data.addrs->nelts = 0;
	instance->state.data.names->nelts = 0;
	instance->state.data.sockaddrs->nelts = 0;
	ngx_memzero(addr, sizeof(ngx_addr_t) * instance->conf.max_ips);
	ngx_memzero(name, NGX_SOCKADDR_STRLEN * instance->conf.max_ips);
	ngx_memzero(sockaddr, NGX_SOCKADDRLEN * instance->conf.max_ips);
	for (i = 0; i < ngx_min(ctx->naddrs, instance->conf.max_ips); i++) {
		ngx_memcpy((sockaddr + (i * NGX_SOCKADDRLEN)), ctx->addrs[i].sockaddr, ctx->addrs[i].socklen);
		ngx_inet_set_port(sockaddr + (i * NGX_SOCKADDRLEN), instance->conf.port);
		addr[i].sockaddr = sockaddr + (i * NGX_SOCKADDRLEN);
		addr[i].socklen = ctx->addrs[i].socklen;
		addr[i].name.data = name + (i * NGX_SOCKADDR_STRLEN);
		addr[i].name.len = ngx_sock_ntop(addr[i].sockaddr, addr[i].socklen, addr[i].name.data, NGX_SOCKADDR_STRLEN, 1);
		peerp[i]->down = 0;
		peerp[i]->name.len = addr[i].name.len;
		peerp[i]->socklen = ctx->addrs[i].socklen;
		instance->state.data.server->down = 0;
		instance->state.data.server->naddrs++;
	}
	instance->state.data.naddrs = instance->state.data.server->naddrs;
	for (i = instance->state.data.naddrs; i < instance->conf.max_ips; i++) {
		ngx_memcpy(&addr[i], &INVALID_ADDR, sizeof(ngx_addr_t));
		peerp[i]->down = 1;
		peerp[i]->name.len = addr[i].name.len;
		peerp[i]->socklen = addr[i].socklen;
	}

end:
	ngx_resolve_name_done(ctx);

	instance->state.resolve.access = ngx_time();
	instance->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;
}

/*
 * Called before the main entry point. This function initializes the module state data.
 */
static void *
ngx_http_upstream_jdomain_create_conf(ngx_conf_t *cf)
{
	ngx_http_upstream_jdomain_srv_conf_t *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_jdomain_srv_conf_t));
	if (!conf) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_pcalloc conf fail");
		return NULL;
	}

	conf->instances = ngx_array_create(cf->pool, 1, sizeof(ngx_http_upstream_jdomain_instance_t));
	if (!conf->instances) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create instances fail");
		return NULL;
	}

	return conf;
}

static void *
ngx_http_upstream_jdomain_create_instance(ngx_conf_t *cf, ngx_array_t *instance_array)
{
	ngx_http_upstream_jdomain_instance_t *instance;

	instance = ngx_array_push(instance_array);
	if (instance == NULL) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push instances fail");
		return NULL;
	}
	ngx_memzero(instance, sizeof(ngx_http_upstream_jdomain_instance_t));

	instance->conf.interval = 1;
	instance->conf.max_ips = 4;
	instance->conf.port = 80;

	instance->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;

	return instance;
}

/*
 * Module entrypoint. This function performs initial (blocking) domain name resolution and finishes initializing state data.
 */
static char *
ngx_http_upstream_jdomain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_upstream_srv_conf_t *uscf;
	ngx_http_upstream_jdomain_srv_conf_t *jcf;
	ngx_http_upstream_jdomain_instance_t *instance;

	ngx_addr_t *addr;
	u_char *name, *errstr;
	ngx_http_upstream_rr_peer_t **peerps;
	struct sockaddr *sockaddr;

	ngx_pool_t *pool;
	ngx_str_t *value, s;
	ngx_int_t num;
	ngx_url_t u;
	ngx_uint_t i;
	char *rc;

	INVALID_ADDR_SOCKADDR_IN.sin_addr.s_addr = htonl(INADDR_ANY);
	INVALID_ADDR_SOCKADDR_IN.sin_family = AF_INET;
	INVALID_ADDR_SOCKADDR_IN.sin_port = htons(0);

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

	uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
	jcf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstream_jdomain_module);

	jcf->handlers.default_init =
	  (uscf->peer.init_upstream && uscf->peer.init_upstream != ngx_http_upstream_init_jdomain)
	    ? uscf->peer.init_upstream
	    : (jcf->handlers.default_init ? jcf->handlers.default_init : ngx_http_upstream_init_round_robin);

	uscf->peer.init_upstream = ngx_http_upstream_init_jdomain;
	uscf->flags = NGX_HTTP_UPSTREAM_CREATE | NGX_HTTP_UPSTREAM_WEIGHT | NGX_HTTP_UPSTREAM_MAX_FAILS |
	              NGX_HTTP_UPSTREAM_FAIL_TIMEOUT | NGX_HTTP_UPSTREAM_DOWN | NGX_HTTP_UPSTREAM_BACKUP |
	              NGX_HTTP_UPSTREAM_MAX_CONNS;

	instance = ngx_http_upstream_jdomain_create_instance(cf, jcf->instances);
	if (!instance) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_http_upstream_jdomain_create_instance fail");
		goto failure;
	}
	instance->parent = uscf;

	rc = NGX_CONF_OK;

	value = cf->args->elts;

	/* Parse arguments */
	for (i = 2; i < cf->args->nelts; i++) {
		if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
			s.len = value[i].len - 9;
			s.data = &value[i].data[9];
			instance->conf.interval = ngx_parse_time(&s, 1);
			if (instance->conf.interval < 1) {
				goto invalid;
			}
			continue;
		}

		if (ngx_strncmp(value[i].data, "max_ips=", 8) == 0) {
			num = ngx_atoi(value[i].data + 8, value[i].len - 8);
			if (num < 1) {
				goto invalid;
			}
			instance->conf.max_ips = num;
			continue;
		}

		if (ngx_strncmp(value[i].data, "port=", 5) == 0) {
			num = ngx_atoi(value[i].data + 5, value[i].len - 5);
			if (num < 1 || num != (in_port_t)num) {
				goto invalid;
			}
			instance->conf.port = num;
			continue;
		}

		if (value[i].len == 6 && ngx_strncmp(value[i].data, "strict", 6) == 0) {
			instance->conf.strict = 1;
			continue;
		}

		goto invalid;
	}

	instance->conf.domain.data = value[1].data;
	instance->conf.domain.len = value[1].len;

	/* Initialize state data */
	instance->state.data.sockaddrs = !instance->state.data.sockaddrs
	                                   ? ngx_array_create(cf->pool, instance->conf.max_ips, NGX_SOCKADDRLEN)
	                                   : instance->state.data.sockaddrs;
	if (!instance->state.data.sockaddrs) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create sockaddrs fail");
		goto failure;
	}
	sockaddr = instance->state.data.sockaddrs->elts;
	ngx_memzero(sockaddr, NGX_SOCKADDRLEN * instance->conf.max_ips);

	instance->state.data.names = !instance->state.data.names
	                               ? ngx_array_create(cf->pool, instance->conf.max_ips, NGX_SOCKADDR_STRLEN)
	                               : instance->state.data.names;
	if (!instance->state.data.names) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create names fail");
		goto failure;
	}
	name = instance->state.data.names->elts;
	ngx_memzero(name, NGX_SOCKADDR_STRLEN * instance->conf.max_ips);

	instance->state.data.addrs = !instance->state.data.addrs
	                               ? ngx_array_create(cf->pool, instance->conf.max_ips, sizeof(ngx_addr_t))
	                               : instance->state.data.addrs;
	if (!instance->state.data.addrs) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create addrs fail");
		goto failure;
	}
	addr = instance->state.data.addrs->elts;
	ngx_memzero(addr, sizeof(ngx_addr_t) * instance->conf.max_ips);

	instance->state.data.peerps = !instance->state.data.peerps
	                                ? ngx_array_create(cf->pool, instance->conf.max_ips, sizeof(ngx_http_upstream_rr_peer_t *))
	                                : instance->state.data.peerps;
	if (!instance->state.data.peerps) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create peerps fail");
		goto failure;
	}
	peerps = instance->state.data.peerps->elts;
	ngx_memzero(peerps, sizeof(ngx_http_upstream_rr_peer_t *) * instance->conf.max_ips);

	if (!uscf->servers) {
		uscf->servers = ngx_array_create(cf->pool, 1, sizeof(ngx_http_upstream_server_t));
		if (!uscf->servers) {
			ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create servers fail");
			goto failure;
		}
	}
	instance->state.data.server = ngx_array_push(uscf->servers);
	if (!instance->state.data.server) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push server fail");
		goto failure;
	}
	ngx_memzero(instance->state.data.server, sizeof(ngx_http_upstream_server_t));
	instance->state.data.server->addrs = instance->state.data.addrs->elts;
	instance->state.data.server->fail_timeout = 10; /* server default */
	instance->state.data.server->max_fails = 1;     /* server default */
	instance->state.data.server->name = instance->conf.domain;
	instance->state.data.server->weight = 1; /* server default */

	ngx_memzero(&u, sizeof(ngx_url_t));
	u.url = value[1];
	u.default_port = instance->conf.port;

	/* Initial domain name resolution */
	if (ngx_parse_url(pool, &u) != NGX_OK) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: %s in upstream \"%V\"", u.err ? u.err : "error", &u.url);
		if (uscf->servers->nelts < 2) {
			goto failure;
		}
		ngx_conf_log_error(NGX_LOG_WARN, cf, 0, (const char *)errstr);
	}

	for (i = 0; i < ngx_min(u.naddrs, instance->conf.max_ips); i++) {
		ngx_memcpy((sockaddr + (i * NGX_SOCKADDRLEN)), u.addrs[i].sockaddr, u.addrs[i].socklen);
		ngx_memcpy((name + (i * NGX_SOCKADDR_STRLEN)), u.addrs[i].name.data, u.addrs[i].name.len);
		addr[i].sockaddr = sockaddr + (i * NGX_SOCKADDRLEN);
		addr[i].socklen = u.addrs[i].socklen;
		addr[i].name.data = name + (i * NGX_SOCKADDR_STRLEN);
		addr[i].name.len = u.addrs[i].name.len;
		instance->state.data.server->naddrs++;
	}
	instance->state.data.naddrs = instance->state.data.server->naddrs;
	for (i = instance->state.data.naddrs; i < instance->conf.max_ips; i++) {
		ngx_memcpy(&addr[i], &INVALID_ADDR, sizeof(ngx_addr_t));
	}

	/* This is a hack to allow nginx to load without complaint in case there are other server directives in the upstream block */
	instance->state.data.server->down = instance->state.data.server->naddrs < 1;

	/* This is a hack to guarantee the creation of enough round robin peers up front so we can minimize memory manipulation */
	instance->state.data.server->naddrs = instance->conf.max_ips;

	instance->state.resolve.access = ngx_time();

	goto end;

invalid:
	ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: invalid parameter \"%V\"", &value[i]);

failure:
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, (const char *)errstr);
	rc = (char *)errstr;

end:
	ngx_destroy_pool(pool);
	return rc;
}
