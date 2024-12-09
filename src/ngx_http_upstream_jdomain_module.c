#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_JDOMAIN_STATUS_DONE 0
#define NGX_JDOMAIN_STATUS_WAIT 1

#define NGX_JDOMAIN_DEFAULT_SERVER_FAIL_TIMEOUT 10
#define NGX_JDOMAIN_DEFAULT_SERVER_MAX_FAILS 1
#define NGX_JDOMAIN_DEFAULT_SERVER_WEIGHT 1
#define NGX_JDOMAIN_DEFAULT_PORT 80
#define NGX_JDOMAIN_DEFAULT_PORT_LEN 2

#define NGX_JDOMAIN_IPV4 4
#define NGX_JDOMAIN_IPV6 6

#define NGX_JDOMAIN_FAMILY_DEFAULT 0
#define NGX_JDOMAIN_FAMILY_IPV4 AF_INET
#define NGX_JDOMAIN_FAMILY_IPV6 AF_INET6

#define NGX_JDOMAIN_ARG_STR_INTERVAL "interval="
#define NGX_JDOMAIN_ARG_STR_MAX_IPS "max_ips="
#define NGX_JDOMAIN_ARG_STR_PORT "port="
#define NGX_JDOMAIN_ARG_STR_IPVER "ipver="
#define NGX_JDOMAIN_ARG_STR_MAX_CONNS "max_conns="
#define NGX_JDOMAIN_ARG_STR_STRICT "strict"

typedef struct
{
	struct
	{
		ngx_str_t domain;
		time_t interval;
		ngx_uint_t max_ips;
		in_port_t port;
		ngx_uint_t strict;
		short addr_family; /* ipver= */
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
		ngx_http_upstream_init_pt original_init;
		ngx_http_upstream_init_peer_pt original_init_peer;
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
ngx_http_upstream_jdomain_create_instance(ngx_conf_t *cf, ngx_http_upstream_jdomain_srv_conf_t *conf);

static ngx_int_t
ngx_http_upstream_jdomain_init_instance_data(ngx_conf_t *cf, ngx_http_upstream_jdomain_instance_t *instance);

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

static struct sockaddr_in NGX_JDOMAIN_INVALID_ADDR_SOCKADDR_IN = { 0 };
static const ngx_addr_t NGX_JDOMAIN_INVALID_ADDR = { (struct sockaddr *)(&NGX_JDOMAIN_INVALID_ADDR_SOCKADDR_IN),
	                                                   sizeof(struct sockaddr_in),
	                                                   ngx_string("NGX_UPSTREAM_JDOMAIN_BUFFER") };

/*
 * The upstream initialization handler, called once at the beginning of nginx runtime to initialize the module. We register
 * this function to hook into the upstream initialization and discover load balancer peers, since those are derived from the
 * upstream's server structure and used to actually connect with upstreams.
 */
static ngx_int_t
ngx_http_upstream_init_jdomain(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
	ngx_http_upstream_jdomain_srv_conf_t *jcf;
	ngx_http_upstream_jdomain_instance_t *instance;
	ngx_http_upstream_rr_peer_t *peer;
	ngx_http_upstream_rr_peer_t **peerp;
	ngx_http_upstream_rr_peers_t *peers;
	ngx_uint_t i;
	ngx_uint_t j;

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0, "ngx_http_upstream_jdomain_module: init jdomain");

	jcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_jdomain_module);

	/* Call original init handler */
	if (jcf->handlers.original_init(cf, us) != NGX_OK) {
		return NGX_ERROR;
	}

	/* Intercept upstream peer init handler */
	jcf->handlers.original_init_peer = us->peer.init;
	us->peer.init = ngx_http_upstream_init_jdomain_peer;

	instance = jcf->instances->elts;
	peers = us->peer.data;
	/* Discover and cache peer pointers which are derived from our jdomain addresses */
	for (i = 0; i < jcf->instances->nelts; i++) {
		peerp = instance[i].state.data.peerps->elts;
		j = 0;

		for (peer = peers->peer; peer; peer = peer->next) {
			if (instance[i].state.data.server->name.len == peer->server.len &&
			    ngx_strncmp(instance[i].state.data.server->name.data, peer->server.data, peer->server.len) == 0) {
				peerp[j++] = peer;
			}
		}

		if (j != instance[i].conf.max_ips) {
			ngx_conf_log_error(NGX_LOG_EMERG,
			                   cf,
			                   0,
			                   "ngx_http_upstream_jdomain_module: num peerps (%d) does not match max_ips (%d)",
			                   j,
			                   instance[i].conf.max_ips);
			return NGX_ERROR;
		}

		/* In the case we resolve less IPs than max_ips, we set the remaining peers to down state */
		for (j = instance[i].state.data.naddrs; j < instance[i].conf.max_ips; j++) {
			peerp[j]->down = 1;
		}

		/* Now we can undo our hack from the module entry point and store the correct number of addresses */
		instance[i].state.data.server->naddrs = instance[i].state.data.naddrs;
	}

	return NGX_OK;
}

/*
 * The upstream peer initialization handler, called per request to this upstream. We register this function to hook into the
 * upstream peer initialization so we can check if the intervals have elapsed to resolve any of the domains associated with our
 * jdomain instances. If it is the case, then we attempt to initiate a DNS resolution using the configured resolver.
 */
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

	/* Call original init peer handler */
	rc = jcf->handlers.original_init_peer(r, us);
	if (rc != NGX_OK) {
		goto end;
	}

	clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
	instance = jcf->instances->elts;
	for (i = 0; i < jcf->instances->nelts; i++) {
		/* Check if this jdomain instance is in the middle of processing a domain resolve or if it's not time yet to start one */
		if (instance[i].state.resolve.status == NGX_JDOMAIN_STATUS_WAIT ||
		    ngx_time() <= instance[i].state.resolve.access + instance[i].conf.interval) {
			continue;
		}

		ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "ngx_http_upstream_jdomain_module: update from DNS cache");

		/* Initialize resolver context with our domain name, a handler function, and our configured timeout */
		ctx = ngx_resolve_start(clcf->resolver, NULL);
		if (!ctx) {
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

		/* Initiate the domain name resolution and mark this instance as waiting */
		rc = ngx_resolve_name(ctx);
		if (rc != NGX_OK) {
			ngx_log_error(
			  NGX_LOG_ALERT, r->connection->log, 0, "ngx_http_upstream_jdomain_module: ngx_resolve_name \"%V\" fail", &ctx->name);
			continue;
		}
		if (ctx->state == NGX_AGAIN) {
			instance[i].state.resolve.status = NGX_JDOMAIN_STATUS_WAIT;
		}
	}

end:
	return rc;
}

/*
 * The resolver context handler, called on completion of the domain name resolution attempt. We register this function to
 * update the state data of a jdomain instance by overwriting the cached IP addresses with those newly resolved.
 */
static void
ngx_http_upstream_jdomain_resolve_handler(ngx_resolver_ctx_t *ctx)
{
	ngx_http_upstream_jdomain_instance_t *instance;
	ngx_uint_t i;
	ngx_uint_t f;
	ngx_uint_t exists_alt_server;
	ngx_uint_t naddrs_prev;
	ngx_http_upstream_server_t *server;
	ngx_sockaddr_t *sockaddr;
	u_char *name;
	ngx_addr_t *addr;
	ngx_http_upstream_rr_peer_t **peerp;

	instance = (ngx_http_upstream_jdomain_instance_t *)ctx->data;

	exists_alt_server = 0;
	server = instance->parent->servers->elts;
	for (i = 0; i < instance->parent->servers->nelts; i++) {
		if (&server[i] != instance->state.data.server && !server[i].down) {
			exists_alt_server = 1;
			break;
		}
	}

	/* Determine if there was an error and if so, should we mark the instance as down or not */
	if (ctx->state || ctx->naddrs == 0) {
		instance->state.data.server->down =
		  exists_alt_server && (instance->conf.strict || ctx->state == NGX_RESOLVE_FORMERR || ctx->state == NGX_RESOLVE_NXDOMAIN);
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
	naddrs_prev = instance->state.data.server->naddrs;
	instance->state.data.server->naddrs = 0;
	/* Copy the resolved sockaddrs and address names (IP:PORT) into our state data buffers, marking associated peers up */
	f = 0;
	for (i = 0; i < ctx->naddrs; i++) {
		if (instance->conf.addr_family != NGX_JDOMAIN_FAMILY_DEFAULT &&
		    instance->conf.addr_family != ctx->addrs[i].sockaddr->sa_family) {
			continue;
		}
		addr[f].sockaddr = &sockaddr[f].sockaddr;
		addr[f].socklen = peerp[f]->socklen = ctx->addrs[i].socklen;
		ngx_memcpy(addr[f].sockaddr, ctx->addrs[i].sockaddr, addr[f].socklen);
		ngx_inet_set_port(addr[f].sockaddr, instance->conf.port);
		addr[f].name.data = peerp[f]->name.data = &name[f * NGX_SOCKADDR_STRLEN];
		addr[f].name.len = peerp[f]->name.len =
		  ngx_sock_ntop(addr[f].sockaddr, addr[f].socklen, addr[f].name.data, NGX_SOCKADDR_STRLEN, 1);
		peerp[f]->down = 0;
		instance->state.data.server->down = 0;
		f++;
		if (instance->conf.max_ips == f) {
			break;
		}
	}
	instance->state.data.server->naddrs = f;
	instance->state.data.naddrs = instance->state.data.server->naddrs;
	/* Copy the sockaddr and name of the invalid address (0.0.0.0:0) into the remaining buffers, marking associated peers down */
	for (i = instance->state.data.naddrs; i < ngx_min(naddrs_prev, instance->conf.max_ips); i++) {
		addr[i].name.data = &name[i * NGX_SOCKADDR_STRLEN];
		addr[i].name.len = peerp[i]->name.len = NGX_JDOMAIN_INVALID_ADDR.name.len;
		ngx_memcpy(addr[i].name.data, NGX_JDOMAIN_INVALID_ADDR.name.data, addr[i].name.len);
		addr[i].sockaddr = &sockaddr[i].sockaddr;
		addr[i].socklen = peerp[i]->socklen = NGX_JDOMAIN_INVALID_ADDR.socklen;
		ngx_memcpy(addr[i].sockaddr, NGX_JDOMAIN_INVALID_ADDR.sockaddr, addr[i].socklen);
		peerp[i]->down = 1;
	}

end:
	/* Release the resolver context and mark the completion in the instance state */
	ngx_resolve_name_done(ctx);
	instance->state.resolve.access = ngx_time();
	instance->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;
}

/*
 * Called before the main entry point. This function initializes the module state data structure to be passed to the module
 * entry point by nginx.
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

	return conf;
}

/*
 * This function allocates memory for a jdomain instance. This instance contains the config values from the user as well as
 * pointers to memory buffers for state data.
 */
static void *
ngx_http_upstream_jdomain_create_instance(ngx_conf_t *cf, ngx_http_upstream_jdomain_srv_conf_t *conf)
{
	ngx_http_upstream_jdomain_instance_t *instance;

	conf->instances =
	  conf->instances ? conf->instances : ngx_array_create(cf->pool, 1, sizeof(ngx_http_upstream_jdomain_instance_t));
	if (!conf->instances) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create instances fail");
		return NULL;
	}

	instance = ngx_array_push(conf->instances);
	if (instance == NULL) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push instances fail");
		return NULL;
	}
	ngx_memzero(instance, sizeof(ngx_http_upstream_jdomain_instance_t));

	instance->conf.interval = 1;
	instance->conf.max_ips = 4;
	instance->conf.port = NGX_JDOMAIN_DEFAULT_PORT;
	instance->conf.addr_family = NGX_JDOMAIN_FAMILY_DEFAULT;

	instance->state.resolve.status = NGX_JDOMAIN_STATUS_DONE;

	return instance;
}

/*
 * This function allocates memory for the buffers used for state data of a jdomain instance. The buffers will have a size equal
 * to the configured number of maximum IPs.
 */
static ngx_int_t
ngx_http_upstream_jdomain_init_instance_data(ngx_conf_t *cf, ngx_http_upstream_jdomain_instance_t *instance)
{
	instance->state.data.addrs = ngx_array_create(cf->pool, instance->conf.max_ips, sizeof(ngx_addr_t));
	if (!instance->state.data.addrs) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create addrs fail");
		return NGX_ERROR;
	}
	if (!ngx_array_push_n(instance->state.data.addrs, instance->conf.max_ips)) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push_n addrs fail");
		return NGX_ERROR;
	}
	ngx_memzero(instance->state.data.addrs->elts, sizeof(ngx_addr_t) * instance->conf.max_ips);

	instance->state.data.names = ngx_array_create(cf->pool, instance->conf.max_ips, NGX_SOCKADDR_STRLEN);
	if (!instance->state.data.names) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create names fail");
		return NGX_ERROR;
	}
	if (!ngx_array_push_n(instance->state.data.names, instance->conf.max_ips)) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push_n names fail");
		return NGX_ERROR;
	}
	ngx_memzero(instance->state.data.names->elts, NGX_SOCKADDR_STRLEN * instance->conf.max_ips);

	instance->state.data.peerps = ngx_array_create(cf->pool, instance->conf.max_ips, sizeof(ngx_http_upstream_rr_peer_t *));
	if (!instance->state.data.peerps) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create peerps fail");
		return NGX_ERROR;
	}
	if (!ngx_array_push_n(instance->state.data.peerps, instance->conf.max_ips)) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push_n peerps fail");
		return NGX_ERROR;
	}
	ngx_memzero(instance->state.data.peerps->elts, sizeof(ngx_http_upstream_rr_peer_t *) * instance->conf.max_ips);

	instance->state.data.sockaddrs = ngx_array_create(cf->pool, instance->conf.max_ips, NGX_SOCKADDRLEN);
	if (!instance->state.data.sockaddrs) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_create sockaddrs fail");
		return NGX_ERROR;
	}
	if (!ngx_array_push_n(instance->state.data.sockaddrs, instance->conf.max_ips)) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "ngx_http_upstream_jdomain_module: ngx_array_push_n sockaddrs fail");
		return NGX_ERROR;
	}
	ngx_memzero(instance->state.data.sockaddrs->elts, NGX_SOCKADDRLEN * instance->conf.max_ips);

	return NGX_OK;
}

/*
 * Module entrypoint. This function parses the config directive arguments, performs the initial (blocking) domain name
 * resolution, and finishes initializing state data.
 */
static char *
ngx_http_upstream_jdomain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_upstream_srv_conf_t *uscf;
	ngx_http_upstream_jdomain_srv_conf_t *jcf;
	ngx_http_upstream_jdomain_instance_t *instance;

	ngx_addr_t *addr;
	u_char *errstr;
	u_char *name;
	ngx_uint_t exists_alt_server;
	size_t port_len;
	ngx_http_upstream_server_t *server;
	ngx_http_upstream_server_t *server_iter;
	ngx_sockaddr_t *sockaddr;

	ngx_str_t *value;
	ngx_str_t s;
	size_t arglen;
	ngx_int_t num;
	ngx_url_t u;
	ngx_uint_t i;
	ngx_uint_t f;
	char *rc;

	NGX_JDOMAIN_INVALID_ADDR_SOCKADDR_IN.sin_addr.s_addr = htonl(INADDR_ANY);
	NGX_JDOMAIN_INVALID_ADDR_SOCKADDR_IN.sin_family = AF_INET;
	NGX_JDOMAIN_INVALID_ADDR_SOCKADDR_IN.sin_port = htons(0);

	errstr = ngx_pcalloc(cf->temp_pool, NGX_MAX_CONF_ERRSTR);
	if (!errstr) {
		rc = "ngx_http_upstream_jdomain_module: ngx_pcalloc errstr fail";
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, rc);
		return rc;
	}

	uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);
	jcf = ngx_http_conf_upstream_srv_conf(uscf, ngx_http_upstream_jdomain_module);

	/* Intercept upstream init handler and set upstream flags */
	jcf->handlers.original_init =
	  (uscf->peer.init_upstream && uscf->peer.init_upstream != ngx_http_upstream_init_jdomain)
	    ? uscf->peer.init_upstream
	    : (jcf->handlers.original_init ? jcf->handlers.original_init : ngx_http_upstream_init_round_robin);

	uscf->peer.init_upstream = ngx_http_upstream_init_jdomain;
	uscf->flags = NGX_HTTP_UPSTREAM_CREATE | NGX_HTTP_UPSTREAM_WEIGHT | NGX_HTTP_UPSTREAM_MAX_FAILS |
	              NGX_HTTP_UPSTREAM_FAIL_TIMEOUT | NGX_HTTP_UPSTREAM_DOWN | NGX_HTTP_UPSTREAM_BACKUP |
	              NGX_HTTP_UPSTREAM_MAX_CONNS;

	/* Create underlying upstream server structure and set default values */
	uscf->servers = uscf->servers ? uscf->servers : ngx_array_create(cf->pool, 1, sizeof(ngx_http_upstream_server_t));
	if (!uscf->servers) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_create servers fail");
		goto failure;
	}
	server = ngx_array_push(uscf->servers);
	if (!server) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_array_push server fail");
		goto failure;
	}
	ngx_memzero(server, sizeof(ngx_http_upstream_server_t));
	server->fail_timeout = NGX_JDOMAIN_DEFAULT_SERVER_FAIL_TIMEOUT;
	server->max_fails = NGX_JDOMAIN_DEFAULT_SERVER_MAX_FAILS;
	server->weight = NGX_JDOMAIN_DEFAULT_SERVER_WEIGHT;

	/* Allocate space for state of an instance of a jdomain upstream */
	instance = ngx_http_upstream_jdomain_create_instance(cf, jcf);
	if (!instance) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_http_upstream_jdomain_create_instance fail");
		goto failure;
	}
	instance->parent = uscf;
	instance->state.data.server = server;

	rc = NGX_CONF_OK;

	value = cf->args->elts;
	port_len = NGX_JDOMAIN_DEFAULT_PORT_LEN;

	/* Parse arguments */
	for (i = 2; i < cf->args->nelts; i++) {
		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_INTERVAL);
		if (ngx_strncmp(value[i].data, NGX_JDOMAIN_ARG_STR_INTERVAL, arglen) == 0) {
			s.len = value[i].len - arglen;
			s.data = &value[i].data[arglen];
			instance->conf.interval = ngx_parse_time(&s, 1);
			if (instance->conf.interval < 1) {
				goto invalid;
			}
			continue;
		}

		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_MAX_IPS);
		if (ngx_strncmp(value[i].data, NGX_JDOMAIN_ARG_STR_MAX_IPS, arglen) == 0) {
			num = ngx_atoi(value[i].data + arglen, value[i].len - arglen);
			if (num < 1) {
				goto invalid;
			}
			instance->conf.max_ips = num;
			continue;
		}

		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_PORT);
		if (ngx_strncmp(value[i].data, "port=", arglen) == 0) {
			num = ngx_atoi(value[i].data + arglen, value[i].len - arglen);
			if (num < 1 || num != (in_port_t)num) {
				goto invalid;
			}
			instance->conf.port = num;
			port_len = value[i].len - arglen;
			continue;
		}

		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_IPVER);
		if (ngx_strncmp(value[i].data, NGX_JDOMAIN_ARG_STR_IPVER, arglen) == 0) {
			num = ngx_atoi(value[i].data + arglen, value[i].len - arglen);
			switch (num) {
				case NGX_JDOMAIN_IPV4:
					instance->conf.addr_family = NGX_JDOMAIN_FAMILY_IPV4;
					break;
				case NGX_JDOMAIN_IPV6:
					instance->conf.addr_family = NGX_JDOMAIN_FAMILY_IPV6;
					break;
				case 0:
					// valid as default
					break;
				default:
					goto invalid;
			}

			continue;
		}

		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_STRICT);
		if (value[i].len == arglen && ngx_strncmp(value[i].data, NGX_JDOMAIN_ARG_STR_STRICT, arglen) == 0) {
			instance->conf.strict = 1;
			continue;
		}

		arglen = ngx_strlen(NGX_JDOMAIN_ARG_STR_MAX_CONNS);
		if (ngx_strncmp(value[i].data, NGX_JDOMAIN_ARG_STR_MAX_CONNS, arglen) == 0) {
			num = ngx_atoi(value[i].data + arglen, value[i].len - arglen);
			server->max_conns = num;

			continue;
		}

		goto invalid;
	}

	instance->conf.domain.data = value[1].data;
	instance->conf.domain.len = value[1].len;
	server->name.len = value[1].len + port_len + 1;
	server->name.data =
	  ngx_pcalloc(cf->pool,
	              server->name.len + 1); // + 1 here because valgrind is complaining that there's no \0 at the end otherwise
	ngx_sprintf(server->name.data, "%V:%d", &value[1], instance->conf.port);

	/* Initialize state data */
	if (ngx_http_upstream_jdomain_init_instance_data(cf, instance) != NGX_OK) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: ngx_http_upstream_jdomain_init_instance_data fail");
		goto failure;
	}

	ngx_memzero(&u, sizeof(ngx_url_t));
	u.url = value[1];
	u.default_port = instance->conf.port;

	exists_alt_server = 0;
	server_iter = instance->parent->servers->elts;
	for (i = 0; i < instance->parent->servers->nelts; i++) {
		if (&server_iter[i] != server && !server_iter[i].down) {
			exists_alt_server = 1;
			break;
		}
	}

	/* Initial domain name resolution */
	if (ngx_parse_url(cf->temp_pool, &u) != NGX_OK) {
		ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: %s in upstream \"%V\"", u.err ? u.err : "error", &u.url);
		if (!exists_alt_server) {
			goto failure;
		}
		ngx_conf_log_error(NGX_LOG_WARN, cf, 0, (const char *)errstr);
	}

	addr = server->addrs = instance->state.data.addrs->elts;
	name = instance->state.data.names->elts;
	sockaddr = instance->state.data.sockaddrs->elts;
	/* Copy the resolved sockaddrs and address names (IP:PORT) into our state data buffers */
	f = 0;
	for (i = 0; i < u.naddrs; i++) {
		if (instance->conf.addr_family != NGX_JDOMAIN_FAMILY_DEFAULT &&
		    instance->conf.addr_family != u.addrs[i].sockaddr->sa_family) {
			continue;
		}
		addr[f].name.data = &name[f * NGX_SOCKADDR_STRLEN];
		addr[f].name.len = u.addrs[i].name.len;
		ngx_memcpy(addr[f].name.data, u.addrs[i].name.data, addr[f].name.len);
		addr[f].sockaddr = &sockaddr[f].sockaddr;
		addr[f].socklen = u.addrs[i].socklen;
		ngx_memcpy(addr[f].sockaddr, u.addrs[i].sockaddr, addr[f].socklen);
		f++;
		if (instance->conf.max_ips == f) {
			break;
		}
	}
	instance->state.data.naddrs = server->naddrs = f;
	/* Copy the sockaddr and address name of the invalid address (0.0.0.0:0) into the remaining buffers */
	for (i = instance->state.data.naddrs; i < instance->conf.max_ips; i++) {
		addr[i].name.data = &name[i * NGX_SOCKADDR_STRLEN];
		addr[i].name.len = NGX_JDOMAIN_INVALID_ADDR.name.len;
		ngx_memcpy(addr[i].name.data, NGX_JDOMAIN_INVALID_ADDR.name.data, addr[i].name.len);
		addr[i].sockaddr = &sockaddr[i].sockaddr;
		addr[i].socklen = NGX_JDOMAIN_INVALID_ADDR.socklen;
		ngx_memcpy(addr[i].sockaddr, NGX_JDOMAIN_INVALID_ADDR.sockaddr, addr[i].socklen);
	}

	/* This is a hack to allow nginx to load without complaint in case there are other server directives in the upstream block */
	server->down = server->naddrs < 1;

	/* This is a hack to guarantee the creation of enough round robin peers up front so we can minimize memory manipulation */
	server->naddrs = instance->conf.max_ips;

	instance->state.resolve.access = ngx_time();

	goto end;

invalid:
	ngx_sprintf(errstr, "ngx_http_upstream_jdomain_module: invalid parameter \"%V\"", &value[i]);

failure:
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, (const char *)errstr);
	rc = (char *)errstr;

end:
	return rc;
}
