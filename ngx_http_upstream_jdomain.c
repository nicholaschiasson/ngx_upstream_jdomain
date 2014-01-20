/*
 * this module (C) wudaike
 * this module (C) Baidu, Inc.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define NGX_JDOMAIN_STATS_DONE 0
#define NGX_JDOMAIN_STATS_WAIT 1

typedef struct {
	struct sockaddr	sockaddr;
	socklen_t	socklen;

	ngx_str_t	name;
	u_char		ipstr[NGX_SOCKADDR_STRLEN + 1];
}ngx_sockaddr_t;

typedef struct {
	ngx_sockaddr_t		*resolved_addrs;
	ngx_uint_t		default_port;

	ngx_uint_t		resolved_max_ips;
	ngx_uint_t		resolved_num;
	ngx_str_t		resolved_domain;
	ngx_int_t		resolved_stats;
	ngx_uint_t		resolved_index;
	time_t 			resolved_access;
	time_t			resolved_interval;

	ngx_uint_t		upstream_retry;
} ngx_http_upstream_jdomain_srv_conf_t;

typedef struct {
	ngx_http_upstream_jdomain_srv_conf_t	*conf;
	ngx_http_core_loc_conf_t 		*clcf;
	
	ngx_int_t			current;

} ngx_http_upstream_jdomain_peer_data_t;

static char *ngx_http_upstream_jdomain(ngx_conf_t *cf, ngx_command_t *cmd,
	void *conf);

static void * ngx_http_upstream_jdomain_create_conf(ngx_conf_t *cf);

static ngx_int_t ngx_http_upstream_jdomain_init(ngx_conf_t *cf, 
	ngx_http_upstream_srv_conf_t *us);

static ngx_int_t ngx_http_upstream_jdomain_init_peer(ngx_http_request_t *r,
	ngx_http_upstream_srv_conf_t *us);

static ngx_int_t ngx_http_upstream_jdomain_get_peer(ngx_peer_connection_t *pc,
	void *data);

static void ngx_http_upstream_jdomain_free_peer(ngx_peer_connection_t *pc,
	void *data, ngx_uint_t state);

static void ngx_http_upstream_jdomain_handler(ngx_resolver_ctx_t *ctx);

static ngx_command_t  ngx_http_upstream_jdomain_commands[] = {
	{ngx_string("jdomain"),
	 NGX_HTTP_UPS_CONF|NGX_CONF_1MORE,
	 ngx_http_upstream_jdomain,
	 0,
	 0,
	 NULL },

	 ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_jdomain_module_ctx = {
	NULL,						/* preconfiguration */
	NULL,						/* postconfiguration */

	NULL,						/* create main configuration */
	NULL,						/* init main configuration */

	ngx_http_upstream_jdomain_create_conf,		/* create server configuration */
	NULL,						/* merge server configuration */

	NULL,						/* create location configuration */
	NULL						/* merge location configuration */
};


ngx_module_t  ngx_http_upstream_jdomain_module = {
	NGX_MODULE_V1,
	&ngx_http_upstream_jdomain_module_ctx,		/* module context */
	ngx_http_upstream_jdomain_commands,		/* module directives */
	NGX_HTTP_MODULE,				/* module type */
	NULL,						/* init master */
	NULL,						/* init module */
	NULL,						/* init process */
	NULL,						/* init thread */
	NULL,						/* exit thread */
	NULL,						/* exit process */
	NULL,						/* exit master */
	NGX_MODULE_V1_PADDING
};

static ngx_int_t
ngx_http_upstream_jdomain_init(ngx_conf_t *cf, ngx_http_upstream_srv_conf_t *us)
{
	ngx_http_upstream_jdomain_srv_conf_t	*urcf;

	us->peer.init = ngx_http_upstream_jdomain_init_peer;

	urcf = ngx_http_conf_upstream_srv_conf(us, ngx_http_upstream_jdomain_module);
	urcf->resolved_stats = NGX_JDOMAIN_STATS_DONE;

	return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_jdomain_init_peer(ngx_http_request_t *r,
	ngx_http_upstream_srv_conf_t *us)
{
	ngx_http_upstream_jdomain_peer_data_t	*urpd;
	ngx_http_upstream_jdomain_srv_conf_t	*urcf;

	urcf = ngx_http_conf_upstream_srv_conf(us,
					ngx_http_upstream_jdomain_module);
	
	urpd = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_jdomain_peer_data_t));
	if(urpd == NULL) {
		return NGX_ERROR;
	}
	
	urpd->conf = urcf;
	urpd->clcf = ngx_http_get_module_loc_conf(r, ngx_http_core_module);
	urpd->current = -1;
	
	r->upstream->peer.data = urpd;
	r->upstream->peer.free = ngx_http_upstream_jdomain_free_peer;
	r->upstream->peer.get = ngx_http_upstream_jdomain_get_peer;

	if(urcf->upstream_retry){
		r->upstream->peer.tries = (urcf->resolved_num != 1) ? urcf->resolved_num : 2;
	}else{
		r->upstream->peer.tries = 1;
	}

	return NGX_OK;
}

static ngx_int_t
ngx_http_upstream_jdomain_get_peer(ngx_peer_connection_t *pc, void *data)
{
	ngx_http_upstream_jdomain_peer_data_t	*urpd = data;
	ngx_http_upstream_jdomain_srv_conf_t	*urcf = urpd->conf;
	ngx_resolver_ctx_t			*ctx;
	ngx_sockaddr_t				*peer;
 
	pc->cached = 0;
	pc->connection = NULL;

	if(urcf->resolved_stats == NGX_JDOMAIN_STATS_WAIT){
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0,
			"upstream_jdomain: resolving"); 
		goto assign;
	}

	if(ngx_time() <= urcf->resolved_access + urcf->resolved_interval){
		goto assign;
	}

	ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0,
		"upstream_jdomain: update from DNS cache"); 

	ctx = ngx_resolve_start(urpd->clcf->resolver, NULL);
	if(ctx == NULL) {
		ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0,
			"upstream_jdomain: resolve_start fail"); 
		goto assign;
	}

	if(ctx == NGX_NO_RESOLVER) {
		ngx_log_error(NGX_LOG_ALERT, pc->log, 0,
			"upstream_jdomain: no resolver"); 
		goto assign;
	}

	ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0,
		"upstream_jdomain: resolve_start ok"); 

	ctx->name = urcf->resolved_domain;
	ctx->type = NGX_RESOLVE_A;
	ctx->handler = ngx_http_upstream_jdomain_handler;
	ctx->data = urcf;
	ctx->timeout = urpd->clcf->resolver_timeout;

	urcf->resolved_stats = NGX_JDOMAIN_STATS_WAIT;
	if(ngx_resolve_name(ctx) != NGX_OK) {
		ngx_log_error(NGX_LOG_ALERT, pc->log, 0,
			"upstream_jdomain: resolve name \"%V\" fail", &ctx->name);
	}

assign:
	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
		"upstream_jdomain: resolved_num=%ud", urcf->resolved_num); 

	if(urpd->current == -1){
		urcf->resolved_index = (urcf->resolved_index + 1) % urcf->resolved_num;

		urpd->current = urcf->resolved_index;
	}else{
		urpd->current = (urpd->current + 1) % urcf->resolved_num;
	}

	peer = &(urcf->resolved_addrs[urpd->current]);

	pc->sockaddr = &peer->sockaddr;
	pc->socklen = peer->socklen;
	pc->name = &peer->name;

	ngx_log_debug(NGX_LOG_DEBUG_HTTP, pc->log, 0,
		"upstream_jdomain: upstream to DNS peer (%s:%ud)",
		inet_ntoa(((struct sockaddr_in*)(pc->sockaddr))->sin_addr),
		ntohs((unsigned short)((struct sockaddr_in*)(pc->sockaddr))->sin_port));

	return NGX_OK;
}

static void
ngx_http_upstream_jdomain_free_peer(ngx_peer_connection_t *pc, void *data,ngx_uint_t state)
{
	if(pc->tries > 0)
		pc->tries--;
}

static char *
ngx_http_upstream_jdomain(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
	ngx_http_upstream_srv_conf_t  *uscf;
	ngx_http_upstream_jdomain_srv_conf_t *urcf;

	time_t			interval;
	ngx_str_t		*value, domain, s;
	ngx_int_t		default_port, max_ips;
	ngx_uint_t		retry;
	ngx_sockaddr_t		*paddr;
	ngx_url_t		u;
	ngx_uint_t		i;

	interval = 1;
	default_port = 80;
	max_ips = 20;
	retry = 1;
	domain.data = NULL;
	domain.len = 0;

	uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

	/*Just For Padding,upstream{} need it*/
	if(uscf->servers == NULL) {
		uscf->servers = ngx_array_create(cf->pool, 1,
	                                     sizeof(ngx_http_upstream_server_t));
		if(uscf->servers == NULL) {
			return NGX_CONF_ERROR;
		}
	}
	
	urcf = ngx_http_conf_upstream_srv_conf(uscf,
					ngx_http_upstream_jdomain_module);

	uscf->peer.init_upstream = ngx_http_upstream_jdomain_init;

	value = cf->args->elts;

	for (i=2; i < cf->args->nelts; i++) {
		if (ngx_strncmp(value[i].data, "port=", 5) == 0) {
			default_port = ngx_atoi(value[i].data+5, value[i].len - 5);

			if ( default_port == NGX_ERROR || default_port < 1 ||
							default_port > 65535) {
				goto invalid;
			}

			continue;
		}

		if (ngx_strncmp(value[i].data, "interval=", 9) == 0) {
			s.len = value[i].len - 9;
			s.data = &value[i].data[9];
			
			interval = ngx_parse_time(&s, 1);
			
			if (interval == (time_t) NGX_ERROR) {
				goto invalid;
			}
			
			continue;
		}

		if (ngx_strncmp(value[i].data, "max_ips=", 8) == 0) {
			max_ips = ngx_atoi(value[i].data + 8, value[i].len - 8);

			if ( max_ips == NGX_ERROR || max_ips < 1) {
				goto invalid;
			}

			continue;
		}

		if (ngx_strncmp(value[i].data, "retry_off", 9) == 0) {
			retry = 0;

			continue;
		}
		goto invalid;

	}

	domain.data = value[1].data;
	domain.len  = value[1].len;

	urcf->resolved_addrs = ngx_pcalloc(cf->pool,
			max_ips * sizeof(ngx_sockaddr_t));

	if (urcf->resolved_addrs == NULL) {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
			"ngx_palloc resolved_addrs fail");

		return NGX_CONF_ERROR;
	}

	urcf->resolved_interval = interval;
	urcf->resolved_domain = domain;
	urcf->default_port = default_port;
	urcf->resolved_max_ips = max_ips;
	urcf->upstream_retry = retry;

	ngx_memzero(&u, sizeof(ngx_url_t));
	u.url = value[1];
	u.default_port = urcf->default_port;

	if (ngx_parse_url(cf->pool, &u) != NGX_OK) {
		if (u.err) {
			ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
				"%s in upstream \"%V\"", u.err, &u.url);
		}
	
		return NGX_CONF_ERROR;
	}

	urcf->resolved_num = 0;
	for(i = 0; i < u.naddrs ;i++){
		paddr = &urcf->resolved_addrs[urcf->resolved_num];
		paddr->sockaddr = *(struct sockaddr*)u.addrs[i].sockaddr;
		paddr->socklen = u.addrs[i].socklen; 

		paddr->name = u.addrs[i].name;

		urcf->resolved_num++;

		if( urcf->resolved_num >= urcf->resolved_max_ips)
			break;
	}
	/*urcf->resolved_index = 0;*/
	urcf->resolved_access = ngx_time();

	return NGX_CONF_OK;

invalid:
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
		"invalid parameter \"%V\"", &value[i]);

	return NGX_CONF_ERROR;
}

static void *
ngx_http_upstream_jdomain_create_conf(ngx_conf_t *cf)
{
	ngx_http_upstream_jdomain_srv_conf_t	*conf;

	conf = ngx_pcalloc(cf->pool,
			sizeof(ngx_http_upstream_jdomain_srv_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	return conf;
}

static void
ngx_http_upstream_jdomain_handler(ngx_resolver_ctx_t *ctx)
{
	struct sockaddr_in	taddr;
	in_addr_t		addr;
	ngx_uint_t		i;
	ngx_resolver_t		*r;
	ngx_sockaddr_t		*paddr;
	u_char			*p;

	ngx_http_upstream_jdomain_srv_conf_t *urcf;
	
	r = ctx->resolver;
	urcf = (ngx_http_upstream_jdomain_srv_conf_t *)ctx->data;

	ngx_log_debug3(NGX_LOG_DEBUG_CORE, r->log, 0,
			"upstream_jdomain: \"%V\" resolved state(%i: %s)",
			&ctx->name, ctx->state,
			ngx_resolver_strerror(ctx->state));

	if (ctx->state || ctx->naddrs == 0) {
		ngx_log_error(NGX_LOG_ERR, r->log, 0,
			"upstream_jdomain: resolver failed ,\"%V\" (%i: %s))",
			&ctx->name, ctx->state,
			ngx_resolver_strerror(ctx->state));

		goto end;
	}

	urcf->resolved_num = 0;
	for (i = 0; i < ctx->naddrs; i++) {
		addr = ntohl(ctx->addrs[i]);

		ngx_log_debug4(NGX_LOG_DEBUG_CORE,r->log,0,
					"upstream_jdomain: name was resolved to %ud.%ud.%ud.%ud",
					(addr >> 24) & 0xff, (addr >> 16) & 0xff,
					(addr >> 8) & 0xff, addr & 0xff);
		taddr.sin_family = AF_INET;
		taddr.sin_addr.s_addr = ctx->addrs[i];
		taddr.sin_port = htons(urcf->default_port);

		paddr = &urcf->resolved_addrs[urcf->resolved_num];
		paddr->sockaddr = *(struct sockaddr*)&taddr;
		paddr->socklen = sizeof(struct sockaddr);

		p = ngx_snprintf(paddr->ipstr, NGX_SOCKADDR_STRLEN,
				"%s:%d",
				inet_ntoa(taddr.sin_addr), urcf->default_port);

		*p = '\0';
		paddr->name.data = paddr->ipstr;
		paddr->name.len = ngx_strlen(paddr->ipstr);

		urcf->resolved_num++;

		if( urcf->resolved_num >= urcf->resolved_max_ips)
			break;
	}

end:
	ngx_resolve_name_done(ctx);

	urcf->resolved_access = ngx_time();
	urcf->resolved_stats = NGX_JDOMAIN_STATS_DONE;
}

