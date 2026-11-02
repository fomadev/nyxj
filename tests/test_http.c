#include "../include/nyxj.h"
#include "../include/nyxj_http.h"
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

void suite_http(void) {
    RUN(test_route_match);
    RUN(test_response_helpers);
    RUN(test_server_and_client);
}
