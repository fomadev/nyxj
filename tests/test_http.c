#include "../include/nyxj.h"
#include "../include/nyxj_cluster.h"
#include "../include/nyxj_http.h"
#include "../vendor/mongoose/mongoose.h"
#include "../src/common/thread.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

static void test_route_match(void) {
    nyxj_request req;
    memset(&req, 0, sizeof(req));
    CHECK(nyxj_route_match("/api/user/:id", "/api/user/42", &req));
    CHECK_STR(nyxj_req_param(&req, "id"), "42");
    CHECK(nyxj_route_match("/api/:kind/:id/edit", "/api/post/7/edit", &req));
    CHECK_STR(nyxj_req_param(&req, "kind"), "post");
    CHECK_STR(nyxj_req_param(&req, "id"), "7");
    CHECK(nyxj_route_match("/static/*", "/static/css/app.css", &req));
    CHECK_STR(nyxj_req_param(&req, "*"), "css/app.css");
    CHECK(nyxj_route_match("/health", "/health/", NULL));
    CHECK(!nyxj_route_match("/api/user/:id", "/api/user/", &req));
    CHECK(!nyxj_route_match("/api/user/:id", "/api/user/1/x", &req));
    CHECK(!nyxj_route_match("/api/data", "/api/database", NULL));
}

static void test_response_helpers(void) {
    nyxj_response res;
    memset(&res, 0, sizeof(res));
    nyxj_res_header(&res, "X-Test", "a\r\nInjected: yes");
    CHECK(strstr(res.headers, "X-Test: aInjected: yes\r\n") != NULL); /* CR/LF stripped */
    nyxj_res_error(&res, 404, "not_found", "nothing here");
    CHECK(res.status == 404 && !strcmp(res.content_type, "application/json"));
    CHECK(res.body && strstr(res.body, "\"error\":\"not_found\""));
    free(res.body);

    nyxj_request req;
    memset(&req, 0, sizeof(req));
    req.query = "a=1&name=Ada+L%C3%A9&empty=";
    char buf[32];
    CHECK(nyxj_req_query(&req, "name", buf, sizeof(buf)) && !strcmp(buf, "Ada L\xc3\xa9"));
    CHECK(nyxj_req_query(&req, "empty", buf, sizeof(buf)) && buf[0] == '\0');
    CHECK(!nyxj_req_query(&req, "missing", buf, sizeof(buf)));
}

/* --- Live server + client integration --- */

static void h_hello(nyxj_request* req, nyxj_response* res) {
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "hello", nyxj_req_param(req, "name"));
    nyxj_res_json(res, 200, o);
}

static void h_sum(nyxj_request* req, nyxj_response* res) {
    nyxj_value* doc = nyxj_req_json(req);
    if (!doc) { nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message); return; }
    double s = 0;
    for (int i = 0; doc->type == NYXJ_ARRAY && i < doc->as.array.count; i++) s += doc->as.array.items[i]->as.number;
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_number(o, "sum", s);
    nyxj_res_json(res, 200, o);
}

static bool deny_secret(nyxj_request* req, nyxj_response* res, void* user) {
    (void)user;
    if (strcmp(req->path, "/secret") == 0) {
        nyxj_res_error(res, 403, "forbidden", "blocked by middleware");
        return false;
    }
    return true;
}

static void* run_app(void* arg) {
    nyxj_app_run(arg);
    return NULL;
}

static void test_server_and_client(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://127.0.0.1:0";
    cfg.threads = 2;
    cfg.access_log = false;
    cfg.heartbeat_ms = 0;
    cfg.max_body_size = 4096;
    nyxj_app* app = nyxj_app_create(&cfg);
    CHECK(app != NULL);
    if (!app) return;
    nyxj_route_doc doc = {"Greets someone", NULL, "demo", NULL, "{\"type\":\"object\"}"};
    nyxj_app_route_ex(app, "GET", "/hello/:name", h_hello, NULL, 0, &doc);
    nyxj_app_route_ex(app, "POST", "/sum", h_sum, NULL, NYXJ_ROUTE_ASYNC, NULL);
    nyxj_app_use(app, deny_secret, NULL);
    nyxj_error err;
    CHECK(nyxj_app_listen(app, &err));
    int port = nyxj_app_port(app);
    CHECK(port > 0);
    nyxj_thread t;
    CHECK(nyxj_thread_create(&t, run_app, app));

    char url[128];
    nyxj_http_response resp;
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/hello/NyxJ", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err));
    CHECK(resp.status == 200);
    nyxj_value* v = nyxj_http_response_json(&resp, &err);
    CHECK(v && !strcmp(nyxj_get_string(v, "hello"), "NyxJ"));
    char hv[64];
    CHECK(nyxj_http_response_header(&resp, "server", hv, sizeof(hv)) && strstr(hv, "NyxJ"));
    nyxj_free_value(v);
    nyxj_http_response_free(&resp);

    /* Async route executed on the worker pool */
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/sum", port);
    nyxj_result nums = nyxj_parse("[1,2,3,4.5]");
    CHECK(nyxj_http_post_json(url, nums.root, 5000, &resp, &err));
    nyxj_free_value(nums.root);
    v = nyxj_http_response_json(&resp, &err);
    CHECK(resp.status == 200 && v && nyxj_get_number(v, "sum") == 10.5);
    nyxj_free_value(v);
    nyxj_http_response_free(&resp);

    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    o.url = url;
    o.method = "POST";
    o.body = "[1,";
    o.body_len = 3;
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 400);
    nyxj_http_response_free(&resp);

    char big[5000];
    memset(big, ' ', sizeof(big));
    big[0] = '[';
    big[sizeof(big) - 1] = ']';
    o.body = big;
    o.body_len = sizeof(big);
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 413);
    nyxj_http_response_free(&resp);

    snprintf(url, sizeof(url), "http://127.0.0.1:%d/secret", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 403);
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/nowhere", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 404);
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/sum", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 405);
    nyxj_http_response_free(&resp);

    snprintf(url, sizeof(url), "http://127.0.0.1:%d/health", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 200 && strstr(resp.body, "\"ok\""));
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/ready", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 200);
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/metrics", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 200);
    CHECK(resp.body && strstr(resp.body, "nyxj_http_requests_total{method=\"GET\",route=\"/hello/:name\",status=\"200\"} 1"));
    CHECK(resp.body && strstr(resp.body, "nyxj_build_info"));
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/openapi.json", port);
    CHECK(nyxj_http_get(url, 5000, &resp, &err) && resp.status == 200);
    v = nyxj_http_response_json(&resp, &err);
    CHECK(v && nyxj_get_path(v, "paths./hello/{name}.get") != NULL);
    CHECK(v && !strcmp(nyxj_path_string(v, "paths./hello/{name}.get.parameters[0].name", ""), "name"));
    nyxj_free_value(v);
    nyxj_http_response_free(&resp);

    CHECK(!nyxj_http_get("http://127.0.0.1:1/", 2000, &resp, &err) && err.code != NYXJ_OK);

    nyxj_app_stop(app);
    nyxj_thread_join(t);
    nyxj_app_destroy(app);
}

static void h_ids(nyxj_request* req, nyxj_response* res) {
    char tp[64];
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "id", nyxj_req_id(req));
    nyxj_object_set_string(o, "trace", nyxj_req_trace_id(req));
    if (nyxj_req_traceparent(req, tp, sizeof(tp))) nyxj_object_set_string(o, "outgoing", tp);
    nyxj_res_json(res, 200, o);
}

static void test_hardening(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://127.0.0.1:0";
    cfg.threads = 1;
    cfg.access_log = false;
    cfg.heartbeat_ms = 0;
    cfg.rate_limit_rps = 0.1; /* One token per 10 s: no refill during the test, even under Valgrind */
    cfg.rate_limit_burst = 3;
    cfg.idle_timeout_ms = 300;
    nyxj_app* app = nyxj_app_create(&cfg);
    CHECK(app != NULL);
    if (!app) return;
    nyxj_app_get(app, "/ids", h_ids, NULL);
    nyxj_error err;
    CHECK(nyxj_app_listen(app, &err));
    int port = nyxj_app_port(app);
    nyxj_thread t;
    CHECK(nyxj_thread_create(&t, run_app, app));

    char url[128], hv[128];
    nyxj_http_response resp;
    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/ids", port);
    o.url = url;

    /* 1. Incoming X-Request-Id and traceparent are honoured and echoed. */
    o.headers = "X-Request-Id: abc-123\r\ntraceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01\r\n";
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 200);
    nyxj_value* v = nyxj_http_response_json(&resp, &err);
    CHECK(v && !strcmp(nyxj_get_string(v, "id"), "abc-123"));
    CHECK(v && !strcmp(nyxj_get_string(v, "trace"), "4bf92f3577b34da6a3ce929d0e0e4736"));
    CHECK(v && !strncmp(nyxj_get_string(v, "outgoing"), "00-4bf92f3577b34da6a3ce929d0e0e4736-", 36));
    nyxj_free_value(v);
    CHECK(nyxj_http_response_header(&resp, "x-request-id", hv, sizeof(hv)) && !strcmp(hv, "abc-123"));
    CHECK(nyxj_http_response_header(&resp, "traceparent", hv, sizeof(hv)) &&
          !strncmp(hv, "00-4bf92f3577b34da6a3ce929d0e0e4736-", 36) && strlen(hv) == 55);
    /* Security headers on every response. */
    CHECK(nyxj_http_response_header(&resp, "x-content-type-options", hv, sizeof(hv)) && !strcmp(hv, "nosniff"));
    CHECK(nyxj_http_response_header(&resp, "x-frame-options", hv, sizeof(hv)) && !strcmp(hv, "DENY"));
    nyxj_http_response_free(&resp);

    /* 2. Invalid ids are replaced by generated ones. */
    o.headers = "X-Request-Id: bad id with spaces\r\ntraceparent: 00-00000000000000000000000000000000-00f067aa0ba902b7-01\r\n";
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 200);
    v = nyxj_http_response_json(&resp, &err);
    CHECK(v && strlen(nyxj_get_string(v, "id")) == 32);
    CHECK(v && strlen(nyxj_get_string(v, "trace")) == 32 && strcmp(nyxj_get_string(v, "trace"), "00000000000000000000000000000000"));
    nyxj_free_value(v);
    nyxj_http_response_free(&resp);

    /* 3. Rate limit: burst of 3, the 4th request is rejected with Retry-After. */
    o.headers = NULL;
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 200);
    nyxj_http_response_free(&resp);
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 429);
    CHECK(nyxj_http_response_header(&resp, "retry-after", hv, sizeof(hv)) && atoi(hv) >= 1);
    CHECK(resp.body && strstr(resp.body, "rate_limited"));
    nyxj_http_response_free(&resp);
    /* Another API key has its own bucket; probes are never limited. */
    o.headers = "X-API-Key: tenant-b\r\n";
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 200);
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/health", port);
    o.headers = NULL;
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.status == 200);
    nyxj_http_response_free(&resp);
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/metrics", port);
    CHECK(nyxj_http_request(&o, &resp, &err) && resp.body && strstr(resp.body, "nyxj_http_rate_limited_total 1"));
    nyxj_http_response_free(&resp);

    nyxj_app_stop(app);
    nyxj_thread_join(t);
    nyxj_app_destroy(app);
}

/* Idle keep-alive connections are closed by the server. */
static void idle_client_fn(struct mg_connection* c, int ev, void* ev_data) {
    (void)ev_data;
    int* state = c->fn_data;
    if (ev == MG_EV_CONNECT) *state = 1;
    if (ev == MG_EV_CLOSE) *state = 2;
}

static void test_idle_timeout(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://127.0.0.1:0";
    cfg.threads = -1;
    cfg.access_log = false;
    cfg.heartbeat_ms = 0;
    cfg.idle_timeout_ms = 200;
    nyxj_app* app = nyxj_app_create(&cfg);
    CHECK(app != NULL);
    if (!app) return;
    nyxj_error err;
    CHECK(nyxj_app_listen(app, &err));
    char url[64];
    snprintf(url, sizeof(url), "tcp://127.0.0.1:%d", nyxj_app_port(app));
    struct mg_mgr client;
    mg_mgr_init(&client);
    int state = 0;
    CHECK(mg_connect(&client, url, idle_client_fn, &state) != NULL);
    uint64_t t0 = nyxj_now_ms(), connected_at = 0, closed_at = 0;
    while (nyxj_now_ms() - t0 < 3000 && state != 2) {
        mg_mgr_poll(&client, 5);
        nyxj_app_poll(app, 5);
        if (state == 1 && !connected_at) connected_at = nyxj_now_ms();
    }
    closed_at = nyxj_now_ms();
    CHECK(connected_at != 0);
    CHECK(state == 2);                            /* The server closed the silent connection */
    CHECK(closed_at - connected_at >= 150);       /* ... but not before the timeout */
    mg_mgr_free(&client);
    nyxj_app_destroy(app);
}

/* Two nodes discover each other over binary (MessagePack) gossip. */
static void test_cluster_gossip(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://127.0.0.1:0";
    cfg.threads = -1;
    cfg.access_log = false;
    cfg.heartbeat_ms = 0;
    cfg.idle_timeout_ms = 0;
    nyxj_app* a = nyxj_app_create(&cfg);
    nyxj_app* b = nyxj_app_create(&cfg);
    CHECK(a && b);
    if (!a || !b) { nyxj_app_destroy(a); nyxj_app_destroy(b); return; }
    nyxj_error err;
    const char* seed_a[] = {"udp://127.0.0.1:17946"};
    nyxj_cluster_config ca;
    memset(&ca, 0, sizeof(ca));
    ca.bind_url = "udp://127.0.0.1:17946";
    ca.node_name = "a";
    ca.gossip_interval_ms = 50;
    nyxj_cluster_config cb = ca;
    cb.bind_url = "udp://127.0.0.1:17947";
    cb.node_name = "b";
    cb.seeds = seed_a;
    cb.seed_count = 1;
    nyxj_cluster* na = nyxj_cluster_start(a, &ca, &err);
    nyxj_cluster* nb = nyxj_cluster_start(b, &cb, &err);
    CHECK(na && nb);
    uint64_t t0 = nyxj_now_ms();
    while (na && nb && nyxj_now_ms() - t0 < 10000 &&
           (nyxj_cluster_alive_count(na) < 2 || nyxj_cluster_alive_count(nb) < 2)) {
        nyxj_app_poll(a, 5);
        nyxj_app_poll(b, 5);
    }
    CHECK(na && nyxj_cluster_alive_count(na) == 2);
    CHECK(nb && nyxj_cluster_alive_count(nb) == 2);
    nyxj_app_destroy(a);
    nyxj_app_destroy(b);
}

void suite_http(void) {
    RUN(test_route_match);
    RUN(test_response_helpers);
    RUN(test_server_and_client);
    RUN(test_hardening);
    RUN(test_idle_timeout);
    RUN(test_cluster_gossip);
}
