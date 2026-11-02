/* Internal definition of nyxj_app, shared by the networking sources. */
#ifndef NYXJ_APP_INTERNAL_H
#define NYXJ_APP_INTERNAL_H

#include "../../vendor/mongoose/mongoose.h"

#include "../../include/nyxj_http.h"
#include "../../include/nyxj_log.h"
#include "../../include/nyxj_plugin.h"
#include "../common/thread.h"

typedef struct {
    char* method;
    char* pattern;
    nyxj_handler h;
    void* user;
    unsigned flags;
    char* summary;
    char* description;
    char* tag;
    nyxj_value* request_schema;
    nyxj_value* response_schema;
} nyxj_route;

typedef struct {
    nyxj_middleware fn;
    void* user;
} nyxj_mw;

typedef struct nyxj_job {
    nyxj_app* app;
    unsigned long conn_id;
    nyxj_handler h;
    nyxj_request req;
    nyxj_response res;
    uint64_t start_ns;
    bool head;
    bool close_after;
    struct nyxj_job* next;
} nyxj_job;

typedef struct {
    void* lib;
    char* path;
    const nyxj_plugin_meta* meta;
    nyxj_plugin_shutdown_fn shutdown;
} nyxj_plugin_slot;

typedef struct {
    nyxj_app* app;
    nyxj_timer_fn fn;
    void* user;
} nyxj_timer_slot;

struct nyxj_app {
    nyxj_app_config cfg;
    char* listen_url;
    char* title;
    char* description;
    char* service_version;
    char* heartbeat_url;
    struct mg_mgr mgr;
    struct mg_connection* listener;
    bool listening;
    volatile int stop;
    uint64_t start_ms;

    nyxj_route* routes;
    int nroutes, route_cap;
    nyxj_mw* mws;
    int nmws;
    nyxj_handler fallback;
    void* fallback_user;
    unsigned fallback_flags;

    nyxj_pool* pool;
    nyxj_kv* kv;
    nyxj_metrics* metrics;

    nyxj_mutex done_lock;
    nyxj_job* done_head;
    nyxj_job* done_tail;

    bool tls;
    char* tls_cert;
    size_t tls_cert_len;
    char* tls_key;
    size_t tls_key_len;

    int eco_level;
    int eco_calm_samples;

    nyxj_plugin_slot* plugins;
    int nplugins;
    nyxj_timer_slot* timers[32];
    int ntimers;

    nyxj_host_api host_api;
    void* cluster;  /* nyxj_cluster*, stopped on destroy */
    void* proxy;    /* Safe-Gate proxy state */

    nyxj_metric* m_inflight;
    nyxj_metric* m_duration;
    nyxj_metric* m_rejected;
    nyxj_metric* m_parse_errors;
    nyxj_metric* m_parse_duration;
    nyxj_metric* m_parsed_bytes;
};

/* Mongoose logs follow the NyxJ level: debug traces only at TRACE, errors otherwise. */
static inline void nyxj_sync_mg_log(void) {
    nyxj_log_level l = nyxj_log_get_level();
    mg_log_set(l <= NYXJ_LOG_TRACE ? MG_LL_DEBUG : l <= NYXJ_LOG_ERROR ? MG_LL_ERROR : MG_LL_NONE);
}

/* Implemented in plugin.c / cluster.c, used by app.c */
void nyxj_plugins_unload(nyxj_app* app);
void nyxj_host_api_init(nyxj_app* app);
void nyxj_cluster_free_internal(void* cluster);
void nyxj_proxy_free_internal(void* proxy);

#endif
