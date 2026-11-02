#include "../include/nyxj.h"
#include "../include/nyxj_ipc.h"
#include "../include/nyxj_kv.h"
#include "../include/nyxj_metrics.h"
#include "../include/nyxj_pool.h"
#include "../src/common/thread.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

/* --- Thread pool --- */

typedef struct {
    nyxj_mutex lock;
    int done;
    double sum;
} pool_ctx;

static pool_ctx g_pool_ctx;

static void parse_task(void* arg) {
    char json[64];
    snprintf(json, sizeof(json), "{\"v\":%d}", (int)(intptr_t)arg);
    nyxj_result r = nyxj_parse(json); /* The parser is reentrant */
    nyxj_mutex_lock(&g_pool_ctx.lock);
    g_pool_ctx.done++;
    g_pool_ctx.sum += nyxj_get_number(r.root, "v");
    nyxj_mutex_unlock(&g_pool_ctx.lock);
    nyxj_free_value(r.root);
}

static void test_pool(void) {
    nyxj_mutex_init(&g_pool_ctx.lock);
    nyxj_pool* p = nyxj_pool_create(4, 256);
    CHECK(p != NULL);
    for (int i = 1; i <= 200; i++) CHECK(nyxj_pool_submit(p, parse_task, (void*)(intptr_t)i));
    nyxj_pool_wait(p);
    CHECK(g_pool_ctx.done == 200 && g_pool_ctx.sum == 20100.0);
    nyxj_pool_set_concurrency(p, 1);
    nyxj_pool_stats st;
    nyxj_pool_stats_get(p, &st);
    CHECK(st.threads == 4 && st.concurrency_limit == 1 && st.completed == 200);
    for (int i = 0; i < 10; i++) nyxj_pool_submit(p, parse_task, (void*)(intptr_t)1);
    nyxj_pool_destroy(p); /* Drains even while throttled */
    CHECK(g_pool_ctx.done == 210);

    nyxj_pool* tiny = nyxj_pool_create(1, 1);
    int rejected = 0;
    for (int i = 0; i < 50; i++) rejected += !nyxj_pool_submit(tiny, parse_task, (void*)(intptr_t)0);
    nyxj_pool_destroy(tiny);
    CHECK(rejected > 0); /* Backpressure instead of unbounded growth */
    nyxj_mutex_destroy(&g_pool_ctx.lock);
}

/* --- Key-value store --- */

static void test_kv(void) {
    nyxj_kv* kv = nyxj_kv_create(4);
    nyxj_value* v = nyxj_new_object();
    nyxj_object_set_string(v, "user", "ada");
    CHECK(nyxj_kv_set(kv, "session:1", v, 0));
    CHECK(nyxj_kv_set(kv, "session:2", v, 30));
    nyxj_free_value(v);
    char key[32];
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "k%d", i);
        nyxj_value* n = nyxj_new_number(i);
        nyxj_kv_set(kv, key, n, 0);
        nyxj_free_value(n);
    }
    CHECK(nyxj_kv_count(kv) == 102);
    nyxj_value* got = nyxj_kv_get(kv, "session:1");
    CHECK(got && !strcmp(nyxj_get_string(got, "user"), "ada"));
    nyxj_free_value(got);
    CHECK(nyxj_kv_ttl(kv, "session:1") == -1);
    CHECK(nyxj_kv_ttl(kv, "missing") == -2);
    double r;
    CHECK(nyxj_kv_incr(kv, "hits", 2, &r) && r == 2);
    CHECK(nyxj_kv_incr(kv, "hits", 3, &r) && r == 5);
    CHECK(!nyxj_kv_incr(kv, "session:1", 1, &r)); /* Not a number */
    nyxj_value* keys = nyxj_kv_keys(kv, "session:");
    CHECK(keys && keys->as.array.count == 2 && !strcmp(keys->as.array.items[0]->as.string, "session:1"));
    nyxj_free_value(keys);
    nyxj_sleep_ms(60);
    CHECK(!nyxj_kv_exists(kv, "session:2")); /* Expired */
    CHECK(nyxj_kv_del(kv, "k5") && !nyxj_kv_del(kv, "k5"));

    nyxj_error err;
    const char* snap = "nyxj_test_snapshot.json";
    CHECK(nyxj_kv_save(kv, snap, &err));
    nyxj_kv* kv2 = nyxj_kv_create(0);
    CHECK(nyxj_kv_load(kv2, snap, &err));
    CHECK(nyxj_kv_count(kv2) == nyxj_kv_count(kv));
    got = nyxj_kv_get(kv2, "hits");
    CHECK(got && got->as.number == 5);
    nyxj_free_value(got);
    remove(snap);
    nyxj_kv_stats st;
    nyxj_kv_stats_get(kv, &st);
    CHECK(st.hits >= 1 && st.keys == nyxj_kv_count(kv));
    nyxj_kv_destroy(kv2);
    nyxj_kv_destroy(kv);
}

/* --- Metrics --- */

static void test_metrics(void) {
    nyxj_metrics* m = nyxj_metrics_create();
    nyxj_metric* c = nyxj_metrics_counter(m, "app_requests_total", "Requests", "route=\"/a\"");
    nyxj_metric_inc(c);
    nyxj_metric_add(c, 2);
    nyxj_metric_add(c, -5); /* Ignored: counters only go up */
    CHECK(nyxj_metric_value(c) == 3);
    CHECK(nyxj_metrics_counter(m, "app_requests_total", "Requests", "route=\"/a\"") == c);
    nyxj_metric_inc(nyxj_metrics_counter(m, "app_requests_total", "Requests", "route=\"/b\""));
    nyxj_metric_set(nyxj_metrics_gauge(m, "app_temp", "Temperature", NULL), 42.5);
    nyxj_metric* h = nyxj_metrics_histogram(m, "app_latency_seconds", "Latency", NULL, NULL, 0);
    nyxj_metric_observe(h, 0.003);
    nyxj_metric_observe(h, 0.2);
    char* text = nyxj_metrics_render(m);
    CHECK(text && strstr(text, "# TYPE app_requests_total counter"));
    CHECK(text && strstr(text, "app_requests_total{route=\"/a\"} 3"));
    CHECK(text && strstr(text, "app_requests_total{route=\"/b\"} 1"));
    CHECK(text && strstr(text, "app_temp 42.5"));
    CHECK(text && strstr(text, "app_latency_seconds_bucket{le=\"+Inf\"} 2"));
    CHECK(text && strstr(text, "app_latency_seconds_count 2"));
    free(text);
    char esc[32];
    nyxj_metrics_escape_label("a\"b", esc, sizeof(esc));
    CHECK_STR(esc, "a\\\"b");
    nyxj_metrics_destroy(m);

    nyxj_sys_stats s;
    CHECK(nyxj_sys_stats_get(&s) && s.cpu_count >= 1 && s.rss_bytes > 0);
}

/* --- IPC --- */

static void test_shm(void) {
    nyxj_error err;
    char name[32];
    snprintf(name, sizeof(name), "t%llu", (unsigned long long)(nyxj_now_ns() % 1000000));
    nyxj_shm* prod = nyxj_shm_create(name, 128, &err);
    CHECK(prod != NULL);
    if (!prod) { printf("      shm: %s\n", err.message); return; }
    nyxj_shm* cons = nyxj_shm_open(name, &err);
    CHECK(cons != NULL);
    if (!cons) { nyxj_shm_close(prod); return; }
    CHECK(nyxj_shm_read(cons, NULL) == NULL);
    int written = 0;
    char msg[32];
    for (int i = 0; i < 100; i++) {
        snprintf(msg, sizeof(msg), "{\"seq\":%d}", i);
        if (!nyxj_shm_write(prod, msg, strlen(msg))) break;
        written++;
    }
    CHECK(written > 0 && written < 100); /* Ring is bounded */
    int read = 0;
    size_t len;
    char* got;
    while ((got = nyxj_shm_read(cons, &len)) != NULL) {
        snprintf(msg, sizeof(msg), "{\"seq\":%d}", read);
        CHECK(!strcmp(got, msg));
        free(got);
        read++;
    }
    CHECK(read == written);
    /* Wrap-around after draining */
    for (int i = 0; i < 20; i++) {
        CHECK(nyxj_shm_write(prod, "0123456789", 10));
        got = nyxj_shm_read(cons, &len);
        CHECK(got && len == 10 && !memcmp(got, "0123456789", 10));
        free(got);
    }
    nyxj_shm_close(cons);
    nyxj_shm_close(prod);
}

static void* ipc_server(void* arg) {
    nyxj_ipc* listener = arg;
    nyxj_error err;
    nyxj_ipc* ch = nyxj_ipc_accept(listener, 5000, &err);
    if (!ch) return NULL;
    nyxj_value* req = nyxj_ipc_recv_json(ch, 5000, &err);
    if (req) {
        nyxj_object_set_bool(req, "pong", true);
        nyxj_ipc_send_json(ch, req, &err);
        nyxj_free_value(req);
    }
    nyxj_ipc_close(ch);
    return NULL;
}

static void test_unix_socket(void) {
    nyxj_error err;
    char path[64];
    snprintf(path, sizeof(path), "nyxj_test_%llu.sock", (unsigned long long)(nyxj_now_ns() % 1000000));
    nyxj_ipc* listener = nyxj_ipc_listen(path, &err);
    if (!listener) {
        printf("      unix sockets unavailable here (%s), skipped\n", err.message);
        CHECK(err.code == NYXJ_ERR_UNSUPPORTED || err.code == NYXJ_ERR_IO);
        return;
    }
    nyxj_thread t;
    CHECK(nyxj_thread_create(&t, ipc_server, listener));
    nyxj_ipc* client = nyxj_ipc_connect(path, &err);
    CHECK(client != NULL);
    if (client) {
        nyxj_value* ping = nyxj_new_object();
        nyxj_object_set_string(ping, "msg", "ping");
        CHECK(nyxj_ipc_send_json(client, ping, &err));
        nyxj_free_value(ping);
        nyxj_value* reply = nyxj_ipc_recv_json(client, 5000, &err);
        CHECK(reply && nyxj_get_bool(reply, "pong") && !strcmp(nyxj_get_string(reply, "msg"), "ping"));
        nyxj_free_value(reply);
        nyxj_ipc_close(client);
    }
    nyxj_thread_join(t);
    nyxj_ipc_close(listener);
}

void suite_runtime(void) {
    RUN(test_pool);
    RUN(test_kv);
    RUN(test_metrics);
    RUN(test_shm);
    RUN(test_unix_socket);
}
