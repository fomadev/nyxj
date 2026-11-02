#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../../include/nyxj_log.h"
#include "../common/buf.h"
#include "../core/internal.h"

#include <stdio.h>
#include <string.h>

/*
 * NyxJ Safe-Gate: a JSON firewall placed in front of slower or more fragile
 * back-ends (Node.js, Python, ...). Every unmatched request is checked by the
 * Guardian sanitizer; malicious or malformed payloads are rejected before
 * they ever reach the upstream service, clean ones are forwarded.
 */

typedef struct {
    char* upstream;
    nyxj_gate_policy policy;
} proxy_state;

static const char* forwarded_headers[] = {"Authorization", "Accept", "Accept-Language", "User-Agent", "Cookie",
                                          "X-Request-Id", "X-Correlation-Id"};

static int status_for(const char* reason) {
    if (strcmp(reason, "size_limit") == 0) return 413;
    if (strcmp(reason, "malformed") == 0 || strcmp(reason, "empty_or_truncated") == 0) return 400;
    return 422;
}

static void proxy_handler(nyxj_request* req, nyxj_response* res) {
    proxy_state* st = req->user;
    nyxj_app* app = req->app;
    if (req->body_len > 0) {
        nyxj_gate_verdict v;
        if (!nyxj_gate_check(req->body, req->body_len, &st->policy, &v)) {
            char labels[96];
            snprintf(labels, sizeof(labels), "reason=\"%s\"", v.reason);
            nyxj_metric_inc(nyxj_metrics_counter(app->metrics, "nyxj_gate_blocked_total", "Requests blocked by Safe-Gate", labels));
            NYXJ_WARN("gate", "blocked %s %s from %s: %s", req->method, req->path, req->remote_addr, v.error.message);
            nyxj_res_error(res, status_for(v.reason), v.reason, v.error.message);
            return;
        }
    }
    nyxj_buf url;
    nyxj_buf_init(&url, 256);
    nyxj_buf_puts(&url, st->upstream);
    nyxj_buf_puts(&url, req->path);
    if (req->query && *req->query) {
        nyxj_buf_putc(&url, '?');
        nyxj_buf_puts(&url, req->query);
    }
    nyxj_buf headers;
    nyxj_buf_init(&headers, 256);
    for (size_t i = 0; i < sizeof(forwarded_headers) / sizeof(forwarded_headers[0]); i++) {
        const char* v = nyxj_req_header(req, forwarded_headers[i]);
        if (v) nyxj_buf_printf(&headers, "%s: %s\r\n", forwarded_headers[i], v);
    }
    nyxj_buf_printf(&headers, "X-Forwarded-For: %s\r\nX-NyxJ-Gate: passed\r\n", req->remote_addr);

    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    o.method = req->method;
    o.url = url.data;
    o.headers = headers.data;
    o.body = req->body;
    o.body_len = req->body_len;
    o.content_type = nyxj_req_header(req, "Content-Type");
    o.timeout_ms = 30000;
    nyxj_http_response up;
    nyxj_error err;
    if (url.data && nyxj_http_request(&o, &up, &err)) {
        char ct[96] = "application/octet-stream";
        nyxj_http_response_header(&up, "Content-Type", ct, sizeof(ct));
        nyxj_res_bytes(res, up.status, ct, up.body, up.body_len);
        nyxj_res_header(res, "X-NyxJ-Gate", "passed");
        nyxj_metric_inc(nyxj_metrics_counter(app->metrics, "nyxj_gate_forwarded_total", "Requests forwarded upstream by Safe-Gate", NULL));
        nyxj_http_response_free(&up);
    } else {
        nyxj_metric_inc(nyxj_metrics_counter(app->metrics, "nyxj_gate_upstream_errors_total", "Upstream failures seen by Safe-Gate", NULL));
        nyxj_res_error(res, err.code == NYXJ_ERR_TIMEOUT ? 504 : 502, "bad_gateway", err.message);
    }
    nyxj_buf_free(&url);
    nyxj_buf_free(&headers);
}

bool nyxj_app_enable_gate_proxy(nyxj_app* app, const char* upstream_url, const nyxj_gate_policy* policy) {
    if (!app || !upstream_url) return false;
    proxy_state* st = calloc(1, sizeof(*st));
    if (!st) return false;
    st->upstream = nyxj_strdup(upstream_url);
    if (!st->upstream) { free(st); return false; }
    size_t n = strlen(st->upstream);
    while (n > 0 && st->upstream[n - 1] == '/') st->upstream[--n] = '\0';
    st->policy = policy ? *policy : nyxj_gate_policy_default();
    if (app->proxy) nyxj_proxy_free_internal(app->proxy);
    app->proxy = st;
    nyxj_app_set_fallback(app, proxy_handler, st, NYXJ_ROUTE_ASYNC);
    NYXJ_INFO("gate", "Safe-Gate enabled: unmatched requests are sanitized and forwarded to %s", st->upstream);
    return true;
}

void nyxj_proxy_free_internal(void* proxy) {
    proxy_state* st = proxy;
    if (!st) return;
    free(st->upstream);
    free(st);
}
