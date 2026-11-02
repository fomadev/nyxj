/*
 * NyxJ web framework: Express-style router, middleware, worker-pool
 * execution, HTTPS/TLS, auto-generated OpenAPI documentation, health and
 * readiness probes, Prometheus metrics, Eco-Throttle and a native HTTP
 * client. Networking is powered by Mongoose.
 *
 *   nyxj_app* app = nyxj_app_create(NULL);
 *   nyxj_app_get(app, "/api/user/:id", get_user, NULL);
 *   nyxj_app_listen(app, &err);
 *   nyxj_app_run(app);
 */
#ifndef NYXJ_HTTP_H
#define NYXJ_HTTP_H

#include "nyxj.h"
#include "nyxj_gate.h"
#include "nyxj_kv.h"
#include "nyxj_metrics.h"
#include "nyxj_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NYXJ_MAX_PARAMS 8
#define NYXJ_MAX_HEADERS 40

typedef struct nyxj_app nyxj_app;

typedef struct {
    const char* name;
    const char* value;
} nyxj_header;

typedef struct nyxj_request {
    const char* method;
    const char* path;  /* Decoded path, without the query string */
    const char* query; /* Raw query string ("" when absent) */
    const char* body;  /* NUL-terminated copy of the body */
    size_t body_len;
    const char* remote_addr;
    const char* route; /* Matched route pattern ("" when unmatched) */
    int param_count;
    nyxj_header params[NYXJ_MAX_PARAMS];
    int header_count;
    nyxj_header headers[NYXJ_MAX_HEADERS];
    void* user;    /* Route user data */
    nyxj_app* app;
    /* Internal */
    nyxj_value* json_;
    bool json_parsed_;
    nyxj_error json_error_;
    char* storage_;
    char params_buf_[512];
} nyxj_request;

typedef struct nyxj_response {
    int status;
    char* body; /* Heap buffer owned by the response */
    size_t body_len;
    char content_type[96];
    char headers[1024]; /* Extra headers, "Name: value\r\n" lines */
} nyxj_response;

typedef void (*nyxj_handler)(nyxj_request* req, nyxj_response* res);
/* Return false to stop the chain (the middleware has filled `res`). */
typedef bool (*nyxj_middleware)(nyxj_request* req, nyxj_response* res, void* user);
typedef void (*nyxj_timer_fn)(nyxj_app* app, void* user);

#define NYXJ_ROUTE_ASYNC 1u /* Run the handler on the worker pool */

/* OpenAPI metadata attached to a route (all fields optional). */
typedef struct {
    const char* summary;
    const char* description;
    const char* tag;
    const char* request_schema;  /* JSON Schema as JSON text */
    const char* response_schema; /* JSON Schema as JSON text */
} nyxj_route_doc;

typedef struct {
    const char* listen_url;     /* Default "http://0.0.0.0:8000" */
    int threads;                /* Worker pool size, 0 = one per CPU, -1 = no pool */
    int queue_capacity;         /* Pending async jobs before 503 (default 1024) */
    size_t max_body_size;       /* Default 16 MiB, 413 above */
    nyxj_parse_options parse;   /* Limits applied to request JSON */
    const char* tls_cert_file;  /* PEM certificate: enables HTTPS */
    const char* tls_key_file;   /* PEM private key */
    bool cors;                  /* CORS headers and OPTIONS preflight */
    bool access_log;            /* One INFO line per request */
    bool enable_metrics;        /* GET /metrics and /api/metrics */
    bool enable_health;         /* GET /health and /ready */
    bool enable_docs;           /* GET /openapi.json and /docs */
    const char* title;          /* OpenAPI title */
    const char* description;    /* OpenAPI description */
    const char* service_version;
    double ready_queue_ratio;   /* /ready reports 503 above this queue fill ratio (0.9) */
    int heartbeat_ms;           /* Resource sampling period (default 5000, 0 = off) */
    const char* heartbeat_url;  /* Optional URL receiving a POST heartbeat */
    double eco_temp_c;          /* Eco-Throttle: throttle workers above this CPU temperature (0 = off) */
    double eco_load;            /* Eco-Throttle: throttle above this load per CPU (0 = off) */
} nyxj_app_config;

nyxj_app_config nyxj_app_config_default(void);
nyxj_app*       nyxj_app_create(const nyxj_app_config* config);
void            nyxj_app_destroy(nyxj_app* app);

bool nyxj_app_route(nyxj_app* app, const char* method, const char* pattern, nyxj_handler h, void* user);
bool nyxj_app_route_ex(nyxj_app* app, const char* method, const char* pattern, nyxj_handler h, void* user,
                       unsigned flags, const nyxj_route_doc* doc);
#define nyxj_app_get(app, pattern, h, user)    nyxj_app_route((app), "GET", (pattern), (h), (user))
#define nyxj_app_post(app, pattern, h, user)   nyxj_app_route((app), "POST", (pattern), (h), (user))
#define nyxj_app_put(app, pattern, h, user)    nyxj_app_route((app), "PUT", (pattern), (h), (user))
#define nyxj_app_delete(app, pattern, h, user) nyxj_app_route((app), "DELETE", (pattern), (h), (user))

void nyxj_app_use(nyxj_app* app, nyxj_middleware mw, void* user);
void nyxj_app_set_fallback(nyxj_app* app, nyxj_handler h, void* user, unsigned flags); /* Unmatched requests */
bool nyxj_app_add_timer(nyxj_app* app, unsigned period_ms, nyxj_timer_fn fn, void* user);

bool nyxj_app_listen(nyxj_app* app, nyxj_error* err);
int  nyxj_app_port(const nyxj_app* app); /* Actual port (useful with port 0) */
void nyxj_app_poll(nyxj_app* app, int timeout_ms);
int  nyxj_app_run(nyxj_app* app);  /* Blocks until nyxj_app_stop() */
void nyxj_app_stop(nyxj_app* app); /* Thread-safe and signal-safe */

nyxj_metrics*          nyxj_app_metrics(nyxj_app* app);
nyxj_kv*               nyxj_app_kv(nyxj_app* app);
nyxj_pool*             nyxj_app_pool(nyxj_app* app);
const nyxj_app_config* nyxj_app_config_get(const nyxj_app* app);
double                 nyxj_app_uptime(const nyxj_app* app); /* Seconds */
int                    nyxj_app_eco_level(const nyxj_app* app); /* 0 = normal, 1 = throttled, 2 = critical */
char*                  nyxj_app_openapi(nyxj_app* app);       /* OpenAPI 3.0 document, caller frees */

/* Safe-Gate reverse proxy: unmatched requests are sanitized, then forwarded. */
bool nyxj_app_enable_gate_proxy(nyxj_app* app, const char* upstream_url, const nyxj_gate_policy* policy);

/* Plugins (.so / .dll), see nyxj_plugin.h. */
bool nyxj_app_load_plugin(nyxj_app* app, const char* path, nyxj_error* err);

/* --- Request helpers --- */

const char* nyxj_req_param(const nyxj_request* req, const char* name);
const char* nyxj_req_header(const nyxj_request* req, const char* name);
bool        nyxj_req_query(const nyxj_request* req, const char* name, char* out, size_t out_size);
nyxj_value* nyxj_req_json(nyxj_request* req); /* Lazily parsed, owned by the request */
const nyxj_error* nyxj_req_json_error(const nyxj_request* req);

/* --- Response helpers --- */

void nyxj_res_status(nyxj_response* res, int status);
void nyxj_res_header(nyxj_response* res, const char* name, const char* value);
void nyxj_res_json(nyxj_response* res, int status, nyxj_value* body); /* Takes ownership */
void nyxj_res_json_str(nyxj_response* res, int status, const char* json);
void nyxj_res_text(nyxj_response* res, int status, const char* content_type, const char* text);
void nyxj_res_bytes(nyxj_response* res, int status, const char* content_type, const void* data, size_t len);
void nyxj_res_error(nyxj_response* res, int status, const char* code, const char* message);

/* Route pattern matching ("/api/user/:id", "/static/ *"), exposed for tests. */
bool nyxj_route_match(const char* pattern, const char* path, nyxj_request* req);

/* --- HTTP client --- */

typedef struct {
    const char* method;       /* Default "GET" (or "POST" when a body is given) */
    const char* url;          /* http:// or https:// */
    const char* headers;      /* Extra raw headers, "Name: value\r\n" lines */
    const void* body;
    size_t body_len;
    const char* content_type; /* Default "application/json" when a body is given */
    int timeout_ms;           /* Default 10000 */
    const char* ca_file;      /* PEM CA bundle: enables certificate verification */
} nyxj_http_options;

typedef struct {
    int status;
    char* headers; /* Raw response headers */
    char* body;
    size_t body_len;
} nyxj_http_response;

bool        nyxj_http_request(const nyxj_http_options* opts, nyxj_http_response* resp, nyxj_error* err);
bool        nyxj_http_get(const char* url, int timeout_ms, nyxj_http_response* resp, nyxj_error* err);
bool        nyxj_http_post_json(const char* url, const nyxj_value* body, int timeout_ms, nyxj_http_response* resp,
                                nyxj_error* err);
nyxj_value* nyxj_http_response_json(const nyxj_http_response* resp, nyxj_error* err);
bool        nyxj_http_response_header(const nyxj_http_response* resp, const char* name, char* out, size_t out_size);
void        nyxj_http_response_free(nyxj_http_response* resp);

#ifdef __cplusplus
}
#endif

#endif
