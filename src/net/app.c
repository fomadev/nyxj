#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../../include/nyxj_log.h"
#include "../common/buf.h"
#include "../core/internal.h"

#include <stdio.h>
#include <string.h>

#define MOD "http"

static void builtin_routes(nyxj_app* app);

/* --- Configuration --- */

nyxj_app_config nyxj_app_config_default(void) {
    nyxj_app_config c;
    memset(&c, 0, sizeof(c));
    c.listen_url = "http://0.0.0.0:8000";
    c.threads = 0;
    c.queue_capacity = 1024;
    c.max_body_size = 16u * 1024u * 1024u;
    c.parse = nyxj_parse_options_default();
    c.cors = true;
    c.access_log = true;
    c.enable_metrics = true;
    c.enable_health = true;
    c.enable_docs = true;
    c.title = "NyxJ Micro-service";
    c.description = "API powered by the NyxJ engine";
    c.service_version = NYXJ_VERSION;
    c.ready_queue_ratio = 0.9;
    c.heartbeat_ms = 5000;
    return c;
}

/* --- Metrics collector --- */

static void collect(nyxj_metrics* m, void* user) {
    nyxj_app* app = user;
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_uptime_seconds", "Seconds since the service started", NULL),
                    nyxj_app_uptime(app));
    nyxj_sys_stats st;
    if (nyxj_sys_stats_get(&st)) {
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_process_cpu_seconds_total", "User and system CPU time", NULL), st.cpu_seconds);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_process_resident_memory_bytes", "Resident memory size", NULL), (double)st.rss_bytes);
        if (st.load1 >= 0) nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_system_load1", "1-minute load average", NULL), st.load1);
        if (st.cpu_temp_c >= 0)
            nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_cpu_temperature_celsius", "Hottest thermal zone", NULL), st.cpu_temp_c);
    }
    if (app->pool) {
        nyxj_pool_stats ps;
        nyxj_pool_stats_get(app->pool, &ps);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_pool_threads", "Worker threads", NULL), ps.threads);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_pool_queue_depth", "Jobs waiting for a worker", NULL), ps.queued);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_pool_active_workers", "Workers currently busy", NULL), ps.active);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_pool_concurrency_limit", "Active worker limit (Eco-Throttle)", NULL), ps.concurrency_limit);
        nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_pool_completed_total", "Jobs completed by the pool", NULL), (double)ps.completed);
    }
    nyxj_kv_stats ks;
    nyxj_kv_stats_get(app->kv, &ks);
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_kv_keys", "Keys in the embedded store", NULL), (double)ks.keys);
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_kv_hits_total", "Store lookups that found a key", NULL), (double)ks.hits);
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_kv_misses_total", "Store lookups that missed", NULL), (double)ks.misses);
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_eco_throttle_level", "0 normal, 1 throttled, 2 critical", NULL), app->eco_level);
}

/* --- Lifecycle --- */

static char* dup_or_null(const char* s) { return s ? nyxj_strdup(s) : NULL; }

nyxj_app* nyxj_app_create(const nyxj_app_config* config) {
    nyxj_app* app = calloc(1, sizeof(*app));
    if (!app) return NULL;
    app->cfg = config ? *config : nyxj_app_config_default();
    nyxj_app_config d = nyxj_app_config_default();
    if (!app->cfg.listen_url) app->cfg.listen_url = d.listen_url;
    if (!app->cfg.max_body_size) app->cfg.max_body_size = d.max_body_size;
    if (app->cfg.queue_capacity <= 0) app->cfg.queue_capacity = d.queue_capacity;
    if (app->cfg.ready_queue_ratio <= 0) app->cfg.ready_queue_ratio = d.ready_queue_ratio;
    if (!app->cfg.title) app->cfg.title = d.title;
    if (!app->cfg.description) app->cfg.description = d.description;
    if (!app->cfg.service_version) app->cfg.service_version = d.service_version;
    /* Own every string so callers may pass temporaries. */
    app->listen_url = dup_or_null(app->cfg.listen_url);
    app->title = dup_or_null(app->cfg.title);
    app->description = dup_or_null(app->cfg.description);
    app->service_version = dup_or_null(app->cfg.service_version);
    app->heartbeat_url = dup_or_null(app->cfg.heartbeat_url);
    app->cfg.listen_url = app->listen_url;
    app->cfg.title = app->title;
    app->cfg.description = app->description;
    app->cfg.service_version = app->service_version;
    app->cfg.heartbeat_url = app->heartbeat_url;
    app->cfg.tls_cert_file = NULL;
    app->cfg.tls_key_file = NULL;
    app->start_ms = nyxj_now_ms();
    nyxj_mutex_init(&app->done_lock);
    nyxj_sync_mg_log();
    mg_mgr_init(&app->mgr);
    app->mgr.userdata = app;

    app->kv = nyxj_kv_create(256);
    app->metrics = nyxj_metrics_create();
    if (app->cfg.threads >= 0) app->pool = nyxj_pool_create(app->cfg.threads, app->cfg.queue_capacity);
    if (!app->kv || !app->metrics) { nyxj_app_destroy(app); return NULL; }

    if (config && config->tls_cert_file && config->tls_key_file) {
        nyxj_error err;
        app->tls_cert = nyxj_read_file(config->tls_cert_file, &app->tls_cert_len, &err);
        if (app->tls_cert) app->tls_key = nyxj_read_file(config->tls_key_file, &app->tls_key_len, &err);
        if (!app->tls_cert || !app->tls_key) {
            NYXJ_ERROR(MOD, "TLS disabled: %s", err.message);
        } else {
            app->tls = true;
        }
    }

    nyxj_metrics* m = app->metrics;
    app->m_inflight = nyxj_metrics_gauge(m, "nyxj_http_requests_in_flight", "Requests being processed", NULL);
    app->m_duration = nyxj_metrics_histogram(m, "nyxj_http_request_duration_seconds", "HTTP request latency", NULL, NULL, 0);
    app->m_rejected = nyxj_metrics_counter(m, "nyxj_http_rejected_total", "Requests rejected because the worker queue was full", NULL);
    app->m_parse_errors = nyxj_metrics_counter(m, "nyxj_json_parse_errors_total", "Request bodies that failed to parse", NULL);
    app->m_parse_duration = nyxj_metrics_histogram(m, "nyxj_json_parse_duration_seconds", "Time spent parsing request JSON", NULL, NULL, 0);
    app->m_parsed_bytes = nyxj_metrics_counter(m, "nyxj_json_parsed_bytes_total", "Bytes of request JSON parsed", NULL);
    char labels[160];
    snprintf(labels, sizeof(labels), "version=\"%s\",simd=\"%s\"", NYXJ_VERSION, nyxj_simd_backend());
    nyxj_metric_set(nyxj_metrics_gauge(m, "nyxj_build_info", "NyxJ build information", labels), 1);
    nyxj_metrics_add_collector(m, collect, app);

    nyxj_host_api_init(app);
    builtin_routes(app);
    return app;
}

static void free_request(nyxj_request* req) {
    free(req->storage_);
    nyxj_free_value(req->json_);
    req->storage_ = NULL;
    req->json_ = NULL;
}

static void drain_done(nyxj_app* app, bool send);

void nyxj_app_destroy(nyxj_app* app) {
    if (!app) return;
    nyxj_pool_destroy(app->pool); /* Finishes queued jobs before the manager goes away */
    app->pool = NULL;
    drain_done(app, false);
    mg_mgr_free(&app->mgr);
    if (app->cluster) nyxj_cluster_free_internal(app->cluster); /* After its socket and timer are gone */
    for (int i = 0; i < app->ntimers; i++) free(app->timers[i]);
    for (int i = 0; i < app->nroutes; i++) {
        nyxj_route* r = &app->routes[i];
        free(r->method);
        free(r->pattern);
        free(r->summary);
        free(r->description);
        free(r->tag);
        nyxj_free_value(r->request_schema);
        nyxj_free_value(r->response_schema);
    }
    free(app->routes);
    free(app->mws);
    if (app->proxy) nyxj_proxy_free_internal(app->proxy);
    nyxj_plugins_unload(app);
    nyxj_kv_destroy(app->kv);
    nyxj_metrics_destroy(app->metrics);
    free(app->tls_cert);
    free(app->tls_key);
    free(app->listen_url);
    free(app->title);
    free(app->description);
    free(app->service_version);
    free(app->heartbeat_url);
    nyxj_mutex_destroy(&app->done_lock);
    free(app);
}

/* --- Registration --- */

static nyxj_value* parse_schema(const char* text) {
    if (!text) return NULL;
    nyxj_result r = nyxj_parse(text);
    if (!r.is_valid) NYXJ_WARN(MOD, "ignoring invalid OpenAPI schema: %s", r.error.message);
    return r.root;
}

bool nyxj_app_route_ex(nyxj_app* app, const char* method, const char* pattern, nyxj_handler h, void* user,
                       unsigned flags, const nyxj_route_doc* doc) {
    if (!app || !method || !pattern || !h || pattern[0] != '/') return false;
    if (app->nroutes == app->route_cap) {
        int ncap = app->route_cap ? app->route_cap * 2 : 16;
        nyxj_route* g = realloc(app->routes, sizeof(nyxj_route) * (size_t)ncap);
        if (!g) return false;
        app->routes = g;
        app->route_cap = ncap;
    }
    nyxj_route* r = &app->routes[app->nroutes];
    memset(r, 0, sizeof(*r));
    r->method = nyxj_strdup(method);
    r->pattern = nyxj_strdup(pattern);
    if (!r->method || !r->pattern) { free(r->method); free(r->pattern); return false; }
    for (char* p = r->method; *p; p++)
        if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);
    r->h = h;
    r->user = user;
    r->flags = flags;
    if (doc) {
        r->summary = dup_or_null(doc->summary);
        r->description = dup_or_null(doc->description);
        r->tag = dup_or_null(doc->tag);
        r->request_schema = parse_schema(doc->request_schema);
        r->response_schema = parse_schema(doc->response_schema);
    }
    app->nroutes++;
    return true;
}

bool nyxj_app_route(nyxj_app* app, const char* method, const char* pattern, nyxj_handler h, void* user) {
    return nyxj_app_route_ex(app, method, pattern, h, user, 0, NULL);
}

void nyxj_app_use(nyxj_app* app, nyxj_middleware mw, void* user) {
    if (!app || !mw) return;
    nyxj_mw* g = realloc(app->mws, sizeof(nyxj_mw) * (size_t)(app->nmws + 1));
    if (!g) return;
    app->mws = g;
    app->mws[app->nmws].fn = mw;
    app->mws[app->nmws].user = user;
    app->nmws++;
}

void nyxj_app_set_fallback(nyxj_app* app, nyxj_handler h, void* user, unsigned flags) {
    if (!app) return;
    app->fallback = h;
    app->fallback_user = user;
    app->fallback_flags = flags;
}

static void timer_trampoline(void* arg) {
    nyxj_timer_slot* t = arg;
    t->fn(t->app, t->user);
}

bool nyxj_app_add_timer(nyxj_app* app, unsigned period_ms, nyxj_timer_fn fn, void* user) {
    if (!app || !fn || !period_ms || app->ntimers >= (int)(sizeof(app->timers) / sizeof(app->timers[0]))) return false;
    nyxj_timer_slot* t = malloc(sizeof(*t));
    if (!t) return false;
    t->app = app;
    t->fn = fn;
    t->user = user;
    app->timers[app->ntimers++] = t;
    mg_timer_add(&app->mgr, period_ms, MG_TIMER_REPEAT, timer_trampoline, t);
    return true;
}

/* --- Accessors --- */

nyxj_metrics* nyxj_app_metrics(nyxj_app* app) { return app ? app->metrics : NULL; }
nyxj_kv* nyxj_app_kv(nyxj_app* app) { return app ? app->kv : NULL; }
nyxj_pool* nyxj_app_pool(nyxj_app* app) { return app ? app->pool : NULL; }
const nyxj_app_config* nyxj_app_config_get(const nyxj_app* app) { return app ? &app->cfg : NULL; }
double nyxj_app_uptime(const nyxj_app* app) { return app ? (double)(nyxj_now_ms() - app->start_ms) / 1000.0 : 0; }
int nyxj_app_eco_level(const nyxj_app* app) { return app ? app->eco_level : 0; }

int nyxj_app_port(const nyxj_app* app) {
    if (!app || !app->listener) return 0;
    return (int)mg_ntohs(app->listener->loc.port);
}

/* --- Responses --- */

static const char* status_text(int s) {
    switch (s) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 411: return "Length Required";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return s < 300 ? "OK" : s < 400 ? "Redirect" : s < 500 ? "Client Error" : "Server Error";
    }
}

static void send_response(nyxj_app* app, struct mg_connection* c, nyxj_response* res, bool head) {
    if (res->status == 0) res->status = res->body_len ? 200 : 204;
    bool has_body = res->status != 204 && res->status != 304;
    mg_printf(c, "HTTP/1.1 %d %s\r\n", res->status, status_text(res->status));
    if (has_body) {
        mg_printf(c, "Content-Type: %s\r\nContent-Length: %lu\r\n",
                  res->content_type[0] ? res->content_type : "application/octet-stream", (unsigned long)res->body_len);
    }
    mg_printf(c, "Server: NyxJ/%s\r\n", NYXJ_VERSION);
    if (app->cfg.cors) mg_printf(c, "Access-Control-Allow-Origin: *\r\n");
    mg_printf(c, "%s\r\n", res->headers);
    if (has_body && !head && res->body_len) mg_send(c, res->body, res->body_len);
}

static void finish_request(nyxj_app* app, nyxj_request* req, nyxj_response* res, uint64_t start_ns) {
    double secs = (double)(nyxj_now_ns() - start_ns) / 1e9;
    nyxj_metric_observe(app->m_duration, secs);
    nyxj_metric_add(app->m_inflight, -1);
    char route[160], labels[256];
    nyxj_metrics_escape_label(req->route && *req->route ? req->route : "unmatched", route, sizeof(route));
    snprintf(labels, sizeof(labels), "method=\"%s\",route=\"%s\",status=\"%d\"", req->method, route, res->status);
    nyxj_metric_inc(nyxj_metrics_counter(app->metrics, "nyxj_http_requests_total", "HTTP requests by route and status", labels));
    if (app->cfg.access_log)
        NYXJ_INFO(MOD, "%s %s %d %.3fms %luB", req->method, req->path, res->status, secs * 1000.0, (unsigned long)res->body_len);
    free_request(req);
    free(res->body);
    res->body = NULL;
}

/* --- Async completion --- */

static void job_run(void* arg) {
    nyxj_job* j = arg;
    j->h(&j->req, &j->res);
    nyxj_app* app = j->app;
    nyxj_mutex_lock(&app->done_lock);
    j->next = NULL;
    if (app->done_tail) app->done_tail->next = j;
    else app->done_head = j;
    app->done_tail = j;
    nyxj_mutex_unlock(&app->done_lock);
    if (app->listening) mg_wakeup(&app->mgr, j->conn_id, "1", 1); /* Wake the event loop */
}

static void drain_done(nyxj_app* app, bool send) {
    nyxj_mutex_lock(&app->done_lock);
    nyxj_job* j = app->done_head;
    app->done_head = app->done_tail = NULL;
    nyxj_mutex_unlock(&app->done_lock);
    while (j) {
        nyxj_job* next = j->next;
        struct mg_connection* c = NULL;
        if (send)
            for (c = app->mgr.conns; c && c->id != j->conn_id; c = c->next) {}
        if (c && !c->is_closing) {
            send_response(app, c, &j->res, j->head);
            c->is_resp = 0; /* Let Mongoose parse the next pipelined request */
            if (j->close_after) c->is_draining = 1;
        }
        finish_request(app, &j->req, &j->res, j->start_ns);
        free(j);
        j = next;
    }
}

/* --- Request dispatch --- */

/* Copies a Mongoose string (whose buffer may be NULL when empty) and NUL-terminates it. */
static const char* copy_mg_str(char** cursor, struct mg_str v) {
    char* dst = *cursor;
    if (v.len) memcpy(dst, v.buf, v.len);
    dst[v.len] = '\0';
    *cursor = dst + v.len + 1;
    return dst;
}

static bool build_request(nyxj_app* app, struct mg_connection* c, struct mg_http_message* hm, nyxj_request* req) {
    memset(req, 0, sizeof(*req));
    req->app = app;
    int nh = 0;
    size_t hsize = 0;
    while (nh < MG_MAX_HTTP_HEADERS && nh < NYXJ_MAX_HEADERS && hm->headers[nh].name.len) {
        hsize += hm->headers[nh].name.len + hm->headers[nh].value.len + 2;
        nh++;
    }
    size_t total = hm->method.len + hm->uri.len + hm->query.len + hm->body.len + hsize + 4 + 64;
    char* s = malloc(total);
    if (!s) return false;
    req->storage_ = s;

    req->method = copy_mg_str(&s, hm->method);

    int n = hm->uri.len ? mg_url_decode(hm->uri.buf, hm->uri.len, s, hm->uri.len + 1, 0) : 0;
    if (n < 0) { memcpy(s, hm->uri.buf, hm->uri.len); n = (int)hm->uri.len; }
    s[n] = '\0';
    req->path = s;
    s += hm->uri.len + 1;

    req->query = copy_mg_str(&s, hm->query);
    req->body = copy_mg_str(&s, hm->body);
    req->body_len = hm->body.len;

    for (int i = 0; i < nh; i++) {
        req->headers[i].name = copy_mg_str(&s, hm->headers[i].name);
        req->headers[i].value = copy_mg_str(&s, hm->headers[i].value);
    }
    req->header_count = nh;
    mg_snprintf(s, 64, "%M", mg_print_ip, &c->rem);
    req->remote_addr = s;
    req->route = "";
    return true;
}

static bool method_matches(const char* route_method, const char* method) {
    if (strcmp(route_method, "*") == 0 || strcmp(route_method, method) == 0) return true;
    return strcmp(method, "HEAD") == 0 && strcmp(route_method, "GET") == 0;
}

static void handle_request(nyxj_app* app, struct mg_connection* c, struct mg_http_message* hm) {
    uint64_t start = nyxj_now_ns();
    nyxj_metric_add(app->m_inflight, 1);
    nyxj_request req;
    nyxj_response res;
    memset(&res, 0, sizeof(res));
    if (!build_request(app, c, hm, &req)) {
        mg_http_reply(c, 500, "", "");
        nyxj_metric_add(app->m_inflight, -1);
        return;
    }
    bool head = strcmp(req.method, "HEAD") == 0;
    struct mg_str* conn_hdr = mg_http_get_header(hm, "Connection");
    bool close_after = conn_hdr && mg_strcasecmp(*conn_hdr, mg_str("close")) == 0;

    nyxj_handler h = NULL;
    unsigned flags = 0;

    if (app->cfg.cors && strcmp(req.method, "OPTIONS") == 0) {
        res.status = 204;
        nyxj_res_header(&res, "Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        nyxj_res_header(&res, "Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        nyxj_res_header(&res, "Access-Control-Max-Age", "86400");
        goto respond;
    }
    for (int i = 0; i < app->nmws; i++)
        if (!app->mws[i].fn(&req, &res, app->mws[i].user)) goto respond;

    bool path_matched = false;
    char allow[128] = "";
    for (int i = 0; i < app->nroutes; i++) {
        nyxj_route* r = &app->routes[i];
        if (!nyxj_route_match(r->pattern, req.path, &req)) continue;
        if (method_matches(r->method, req.method)) {
            h = r->h;
            flags = r->flags;
            req.user = r->user;
            req.route = r->pattern;
            break;
        }
        path_matched = true;
        if (strlen(allow) + strlen(r->method) + 3 < sizeof(allow)) {
            if (allow[0]) strcat(allow, ", ");
            strcat(allow, r->method);
        }
    }
    if (!h) {
        req.param_count = 0;
        if (path_matched) {
            nyxj_res_header(&res, "Allow", allow);
            nyxj_res_error(&res, 405, "method_not_allowed", "method not allowed for this route");
            goto respond;
        }
        if (!app->fallback) {
            nyxj_res_error(&res, 404, "not_found", "no route matches this path");
            goto respond;
        }
        h = app->fallback;
        flags = app->fallback_flags;
        req.user = app->fallback_user;
    }

    if ((flags & NYXJ_ROUTE_ASYNC) && app->pool) {
        nyxj_job* j = calloc(1, sizeof(*j));
        if (j) {
            j->app = app;
            j->conn_id = c->id;
            j->h = h;
            j->req = req;
            j->res = res;
            j->start_ns = start;
            j->head = head;
            j->close_after = close_after;
            /* Param/route pointers reference the request itself: re-point them into the copy. */
            for (int i = 0; i < j->req.param_count; i++) {
                j->req.params[i].name = j->req.params_buf_ + (req.params[i].name - req.params_buf_);
                j->req.params[i].value = j->req.params_buf_ + (req.params[i].value - req.params_buf_);
            }
            if (nyxj_pool_submit(app->pool, job_run, j)) return; /* Response sent from drain_done() */
            free(j);
        }
        nyxj_metric_inc(app->m_rejected);
        nyxj_res_header(&res, "Retry-After", "1");
        nyxj_res_error(&res, 503, "overloaded", "the worker queue is full, retry later");
        goto respond;
    }
    h(&req, &res);

respond:
    send_response(app, c, &res, head);
    c->is_resp = 0;
    if (close_after) c->is_draining = 1;
    finish_request(app, &req, &res, start);
}

static void http_fn(struct mg_connection* c, int ev, void* ev_data) {
    nyxj_app* app = c->fn_data;
    if (ev == MG_EV_ACCEPT && app->tls) {
        struct mg_tls_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.cert = mg_str_n(app->tls_cert, app->tls_cert_len);
        opts.key = mg_str_n(app->tls_key, app->tls_key_len);
        mg_tls_init(c, &opts);
    } else if (ev == MG_EV_HTTP_HDRS) {
        struct mg_http_message* hm = ev_data;
        struct mg_str* cl = mg_http_get_header(hm, "Content-Length");
        if (cl && c->data[0] != 'X') {
            unsigned long long n = 0;
            for (size_t i = 0; i < cl->len && cl->buf[i] >= '0' && cl->buf[i] <= '9'; i++) n = n * 10 + (unsigned)(cl->buf[i] - '0');
            if (n > app->cfg.max_body_size) {
                c->data[0] = 'X'; /* Reply only once */
                mg_http_reply(c, 413, "Content-Type: application/json\r\nConnection: close\r\n",
                              "{\"error\":\"payload_too_large\",\"message\":\"body exceeds %lu bytes\",\"status\":413}",
                              (unsigned long)app->cfg.max_body_size);
                c->is_draining = 1;
            }
        }
    } else if (ev == MG_EV_HTTP_MSG && c->data[0] != 'X') {
        handle_request(app, c, ev_data);
    }
}

/* --- Eco-Throttle and heartbeat --- */

typedef struct {
    char* url;
    char* body;
} heartbeat_job;

static void heartbeat_send(void* arg) {
    heartbeat_job* hb = arg;
    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    o.method = "POST";
    o.url = hb->url;
    o.body = hb->body;
    o.body_len = strlen(hb->body);
    o.timeout_ms = 3000;
    nyxj_http_response resp;
    nyxj_error err;
    if (!nyxj_http_request(&o, &resp, &err)) NYXJ_DEBUG("heartbeat", "delivery failed: %s", err.message);
    nyxj_http_response_free(&resp);
    free(hb->url);
    free(hb->body);
    free(hb);
}

static void heartbeat_tick(nyxj_app* app, void* user) {
    (void)user;
    nyxj_kv_sweep(app->kv);
    nyxj_sys_stats st;
    nyxj_sys_stats_get(&st);
    int level = 0;
    if (app->cfg.eco_temp_c > 0 && st.cpu_temp_c >= app->cfg.eco_temp_c)
        level = st.cpu_temp_c >= app->cfg.eco_temp_c + 10 ? 2 : 1;
    if (app->cfg.eco_load > 0 && st.load1 >= 0) {
        double per_cpu = st.load1 / (st.cpu_count > 0 ? st.cpu_count : 1);
        int l = per_cpu >= app->cfg.eco_load * 1.5 ? 2 : per_cpu >= app->cfg.eco_load ? 1 : 0;
        if (l > level) level = l;
    }
    /* Hysteresis: step down only after three calm samples in a row. */
    if (level < app->eco_level && ++app->eco_calm_samples < 3) level = app->eco_level;
    if (level >= app->eco_level) app->eco_calm_samples = 0;
    if (level != app->eco_level) {
        int threads = 0;
        if (app->pool) {
            nyxj_pool_stats ps;
            nyxj_pool_stats_get(app->pool, &ps);
            threads = level == 0 ? 0 : level == 1 ? (ps.threads / 2 > 0 ? ps.threads / 2 : 1) : 1;
            nyxj_pool_set_concurrency(app->pool, threads);
        }
        NYXJ_WARN("eco", "Eco-Throttle level %d -> %d (temp %.1fC, load %.2f, workers %s)", app->eco_level, level,
                  st.cpu_temp_c, st.load1, threads ? "limited" : "all");
        app->eco_level = level;
        app->eco_calm_samples = 0;
    }
    NYXJ_DEBUG("heartbeat", "uptime %.0fs rss %lluB load %.2f eco %d", nyxj_app_uptime(app),
               (unsigned long long)st.rss_bytes, st.load1, app->eco_level);
    if (app->heartbeat_url && app->pool) {
        nyxj_value* o = nyxj_new_object();
        nyxj_object_set_string(o, "service", app->title);
        nyxj_object_set_string(o, "version", app->service_version);
        nyxj_object_set_string(o, "engine", NYXJ_VERSION);
        nyxj_object_set_number(o, "uptime_s", nyxj_app_uptime(app));
        nyxj_object_set_number(o, "rss_bytes", (double)st.rss_bytes);
        nyxj_object_set_number(o, "load1", st.load1);
        nyxj_object_set_number(o, "eco_level", app->eco_level);
        heartbeat_job* hb = calloc(1, sizeof(*hb));
        if (hb) {
            hb->url = nyxj_strdup(app->heartbeat_url);
            hb->body = nyxj_serialize(o);
            if (!hb->url || !hb->body || !nyxj_pool_submit(app->pool, heartbeat_send, hb)) {
                free(hb->url);
                free(hb->body);
                free(hb);
            }
        }
        nyxj_free_value(o);
    }
}

/* --- Event loop --- */

bool nyxj_app_listen(nyxj_app* app, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!app) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    if (app->listening) return true;
    if (!mg_wakeup_init(&app->mgr)) NYXJ_WARN(MOD, "mg_wakeup_init failed: async responses rely on polling");
    app->listener = mg_http_listen(&app->mgr, app->listen_url, http_fn, app);
    if (!app->listener) {
        nyxj_error_set(err, NYXJ_ERR_NETWORK, "cannot listen on %s", app->listen_url);
        return false;
    }
    app->listening = true;
    if (app->cfg.heartbeat_ms > 0) nyxj_app_add_timer(app, (unsigned)app->cfg.heartbeat_ms, heartbeat_tick, NULL);
    NYXJ_INFO(MOD, "listening on %s (port %d, %s, %d routes)", app->listen_url, nyxj_app_port(app),
              app->tls ? "TLS" : "plain HTTP", app->nroutes);
    return true;
}

void nyxj_app_poll(nyxj_app* app, int timeout_ms) {
    if (!app) return;
    mg_mgr_poll(&app->mgr, timeout_ms);
    drain_done(app, true);
}

int nyxj_app_run(nyxj_app* app) {
    if (!app) return 1;
    if (!app->listening) {
        nyxj_error err;
        if (!nyxj_app_listen(app, &err)) {
            NYXJ_ERROR(MOD, "%s", err.message);
            return 1;
        }
    }
    while (!app->stop) nyxj_app_poll(app, 100);
    NYXJ_INFO(MOD, "shutting down");
    if (app->pool) nyxj_pool_wait(app->pool);
    for (int i = 0; i < 5; i++) nyxj_app_poll(app, 10); /* Flush pending responses */
    return 0;
}

void nyxj_app_stop(nyxj_app* app) {
    if (app) app->stop = 1;
}

/* --- OpenAPI --- */

static nyxj_value* json_schema_or(const nyxj_value* s, const char* fallback_type) {
    if (s) return nyxj_clone(s);
    nyxj_value* o = nyxj_new_object();
    if (o && fallback_type) nyxj_object_set_string(o, "type", fallback_type);
    return o;
}

char* nyxj_app_openapi(nyxj_app* app) {
    if (!app) return NULL;
    nyxj_value* doc = nyxj_new_object();
    nyxj_object_set_string(doc, "openapi", "3.0.3");
    nyxj_value* info = nyxj_new_object();
    nyxj_object_set_string(info, "title", app->title);
    nyxj_object_set_string(info, "version", app->service_version);
    nyxj_object_set_string(info, "description", app->description);
    nyxj_object_set(doc, "info", info);
    nyxj_value* paths = nyxj_new_object();
    for (int i = 0; i < app->nroutes; i++) {
        nyxj_route* r = &app->routes[i];
        nyxj_buf p;
        nyxj_buf_init(&p, 64);
        nyxj_value* params = nyxj_new_array();
        for (const char* s = r->pattern; *s;) {
            if (*s == ':' || *s == '*') {
                const char* name = *s == '*' ? "wildcard" : s + 1;
                size_t n = 8;
                if (*s == ':') {
                    const char* e = name;
                    while (*e && *e != '/') e++;
                    n = (size_t)(e - name);
                    s = e;
                } else {
                    s++;
                }
                nyxj_buf_putc(&p, '{');
                nyxj_buf_append(&p, name, n);
                nyxj_buf_putc(&p, '}');
                nyxj_value* prm = nyxj_new_object();
                nyxj_value* nm = nyxj_new_stringn(name, n);
                nyxj_object_set(prm, "name", nm);
                nyxj_object_set_string(prm, "in", "path");
                nyxj_object_set_bool(prm, "required", true);
                nyxj_object_set(prm, "schema", json_schema_or(NULL, "string"));
                nyxj_array_push(params, prm);
            } else {
                nyxj_buf_putc(&p, *s++);
            }
        }
        char* path = nyxj_buf_detach(&p, NULL);
        nyxj_value* item = nyxj_object_detach(paths, path);
        if (!item) item = nyxj_new_object();
        nyxj_value* op = nyxj_new_object();
        nyxj_object_set_string(op, "summary", r->summary ? r->summary : r->pattern);
        if (r->description) nyxj_object_set_string(op, "description", r->description);
        nyxj_value* tags = nyxj_new_array();
        nyxj_array_push(tags, nyxj_new_string(r->tag ? r->tag : "default"));
        nyxj_object_set(op, "tags", tags);
        if (params->as.array.count) nyxj_object_set(op, "parameters", params);
        else nyxj_free_value(params);
        bool has_body = strcmp(r->method, "POST") == 0 || strcmp(r->method, "PUT") == 0 || strcmp(r->method, "PATCH") == 0;
        if (has_body || r->request_schema) {
            nyxj_value* rb = nyxj_new_object();
            nyxj_value* content = nyxj_new_object();
            nyxj_value* media = nyxj_new_object();
            nyxj_object_set(media, "schema", json_schema_or(r->request_schema, NULL));
            nyxj_object_set(content, "application/json", media);
            nyxj_object_set(rb, "content", content);
            nyxj_object_set(op, "requestBody", rb);
        }
        nyxj_value* responses = nyxj_new_object();
        nyxj_value* ok = nyxj_new_object();
        nyxj_object_set_string(ok, "description", "Success");
        nyxj_value* content = nyxj_new_object();
        nyxj_value* media = nyxj_new_object();
        nyxj_object_set(media, "schema", json_schema_or(r->response_schema, NULL));
        nyxj_object_set(content, "application/json", media);
        nyxj_object_set(ok, "content", content);
        nyxj_object_set(responses, "200", ok);
        nyxj_value* bad = nyxj_new_object();
        nyxj_object_set_string(bad, "description", "Error ({\"error\", \"message\", \"status\"})");
        nyxj_object_set(responses, "default", bad);
        nyxj_object_set(op, "responses", responses);
        char method[16];
        size_t k = 0;
        const char* m = strcmp(r->method, "*") == 0 ? "get" : r->method;
        for (; m[k] && k + 1 < sizeof(method); k++) method[k] = (char)(m[k] >= 'A' && m[k] <= 'Z' ? m[k] + 32 : m[k]);
        method[k] = '\0';
        nyxj_object_set(item, method, op);
        if (path) nyxj_object_set(paths, path, item);
        else nyxj_free_value(item);
        free(path);
    }
    nyxj_object_set(doc, "paths", paths);
    char* out = nyxj_serialize_pretty(doc, 2);
    nyxj_free_value(doc);
    return out;
}

/* --- Built-in routes --- */

static void route_health(nyxj_request* req, nyxj_response* res) {
    nyxj_app* app = req->app;
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "status", "ok");
    nyxj_object_set_string(o, "service", app->title);
    nyxj_object_set_string(o, "version", app->service_version);
    nyxj_object_set_string(o, "engine", NYXJ_VERSION);
    nyxj_object_set_number(o, "uptime_s", nyxj_app_uptime(app));
    nyxj_res_json(res, 200, o);
}

static void route_ready(nyxj_request* req, nyxj_response* res) {
    nyxj_app* app = req->app;
    double ratio = 0;
    nyxj_pool_stats ps;
    memset(&ps, 0, sizeof(ps));
    if (app->pool) {
        nyxj_pool_stats_get(app->pool, &ps);
        ratio = ps.queue_capacity ? (double)ps.queued / ps.queue_capacity : 0;
    }
    bool overloaded = ratio >= app->cfg.ready_queue_ratio || app->eco_level >= 2;
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "status", overloaded ? "overloaded" : "ready");
    nyxj_object_set_number(o, "queue_ratio", ratio);
    nyxj_object_set_number(o, "queued", ps.queued);
    nyxj_object_set_number(o, "active_workers", ps.active);
    nyxj_object_set_number(o, "eco_level", app->eco_level);
    if (overloaded) nyxj_res_header(res, "X-NyxJ-Scale-Hint", "scale-out");
    nyxj_res_json(res, overloaded ? 503 : 200, o);
}

static void route_metrics(nyxj_request* req, nyxj_response* res) {
    char* text = nyxj_metrics_render(req->app->metrics);
    nyxj_res_text(res, text ? 200 : 500, "text/plain; version=0.0.4; charset=utf-8", text ? text : "");
    free(text);
}

static void route_openapi(nyxj_request* req, nyxj_response* res) {
    char* text = nyxj_app_openapi(req->app);
    if (!text) { nyxj_res_error(res, 500, "internal", "cannot build the OpenAPI document"); return; }
    nyxj_res_json_str(res, 200, text);
    free(text);
}

static void route_docs(nyxj_request* req, nyxj_response* res) {
    nyxj_buf b;
    nyxj_buf_init(&b, 1024);
    nyxj_buf_puts(&b, "<!doctype html><html><head><meta charset=\"utf-8\"><title>");
    nyxj_buf_puts(&b, req->app->title);
    nyxj_buf_puts(&b,
                  " - API docs</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<link rel=\"stylesheet\" href=\"https://unpkg.com/swagger-ui-dist@5.17.14/swagger-ui.css\">"
                  "</head><body><div id=\"ui\"></div>"
                  "<script src=\"https://unpkg.com/swagger-ui-dist@5.17.14/swagger-ui-bundle.js\"></script>"
                  "<script>SwaggerUIBundle({url:'/openapi.json',dom_id:'#ui'});</script></body></html>");
    nyxj_res_text(res, 200, "text/html; charset=utf-8", b.data);
    nyxj_buf_free(&b);
}

static void builtin_routes(nyxj_app* app) {
    nyxj_route_doc doc;
    memset(&doc, 0, sizeof(doc));
    doc.tag = "operations";
    if (app->cfg.enable_health) {
        doc.summary = "Liveness probe";
        nyxj_app_route_ex(app, "GET", "/health", route_health, NULL, 0, &doc);
        doc.summary = "Readiness probe (503 + X-NyxJ-Scale-Hint when overloaded)";
        nyxj_app_route_ex(app, "GET", "/ready", route_ready, NULL, 0, &doc);
    }
    if (app->cfg.enable_metrics) {
        doc.summary = "Prometheus metrics";
        nyxj_app_route_ex(app, "GET", "/metrics", route_metrics, NULL, 0, &doc);
        nyxj_app_route_ex(app, "GET", "/api/metrics", route_metrics, NULL, 0, &doc);
    }
    if (app->cfg.enable_docs) {
        doc.summary = "OpenAPI 3 document generated from the registered routes";
        nyxj_app_route_ex(app, "GET", "/openapi.json", route_openapi, NULL, 0, &doc);
        doc.summary = "Interactive API documentation (Swagger UI)";
        nyxj_app_route_ex(app, "GET", "/docs", route_docs, NULL, 0, &doc);
    }
}
