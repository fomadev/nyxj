/*
 * Plugin ABI: extend a running NyxJ server with shared libraries (.so on
 * Linux/macOS, .dll on Windows) without recompiling the core.
 *
 * The host passes a function table, so plugins do not need to link against
 * NyxJ at all. A plugin exports:
 *
 *   NYXJ_PLUGIN_EXPORT const nyxj_plugin_meta* nyxj_plugin_describe(void);
 *   NYXJ_PLUGIN_EXPORT int  nyxj_plugin_init(nyxj_app* app, const nyxj_host_api* api);   0 = success
 *   NYXJ_PLUGIN_EXPORT void nyxj_plugin_shutdown(void);                                  optional
 */
#ifndef NYXJ_PLUGIN_H
#define NYXJ_PLUGIN_H

#include "nyxj_http.h"
#include "nyxj_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NYXJ_PLUGIN_API_VERSION 1

#if defined(_WIN32)
#define NYXJ_PLUGIN_EXPORT __declspec(dllexport)
#else
#define NYXJ_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef struct {
    const char* name;
    const char* version;
    const char* description;
    int api_version; /* Must be NYXJ_PLUGIN_API_VERSION */
} nyxj_plugin_meta;

typedef struct nyxj_host_api {
    int api_version;
    const char* engine_version;

    /* Routing */
    bool (*route)(nyxj_app* app, const char* method, const char* pattern, nyxj_handler h, void* user,
                  unsigned flags, const nyxj_route_doc* doc);
    void (*use)(nyxj_app* app, nyxj_middleware mw, void* user);

    /* Requests and responses */
    const char* (*req_param)(const nyxj_request* req, const char* name);
    const char* (*req_header)(const nyxj_request* req, const char* name);
    bool (*req_query)(const nyxj_request* req, const char* name, char* out, size_t out_size);
    nyxj_value* (*req_json)(nyxj_request* req);
    void (*res_json)(nyxj_response* res, int status, nyxj_value* body);
    void (*res_text)(nyxj_response* res, int status, const char* content_type, const char* text);
    void (*res_error)(nyxj_response* res, int status, const char* code, const char* message);
    void (*res_header)(nyxj_response* res, const char* name, const char* value);

    /* JSON */
    nyxj_result (*parse)(const char* json, size_t len, const nyxj_parse_options* opts);
    char* (*serialize)(const nyxj_value* v);
    void (*free_value)(nyxj_value* v);
    nyxj_value* (*new_object)(void);
    nyxj_value* (*new_array)(void);
    nyxj_value* (*new_string)(const char* s);
    nyxj_value* (*new_number)(double n);
    nyxj_value* (*new_bool)(bool b);
    nyxj_value* (*new_null)(void);
    bool (*object_set)(nyxj_value* obj, const char* key, nyxj_value* val);
    bool (*array_push)(nyxj_value* arr, nyxj_value* item);
    nyxj_value* (*get_path)(const nyxj_value* root, const char* path);
    void (*free_mem)(void* p); /* Releases buffers returned by serialize() */

    /* Shared services */
    void (*log)(nyxj_log_level level, const char* module, const char* file, int line, const char* fmt, ...);
    nyxj_kv* kv;
    bool (*kv_set)(nyxj_kv* kv, const char* key, const nyxj_value* value, uint64_t ttl_ms);
    nyxj_value* (*kv_get)(nyxj_kv* kv, const char* key);
    nyxj_metrics* metrics;
    nyxj_metric* (*counter)(nyxj_metrics* m, const char* name, const char* help, const char* labels);
    void (*metric_inc)(nyxj_metric* metric);
} nyxj_host_api;

typedef const nyxj_plugin_meta* (*nyxj_plugin_describe_fn)(void);
typedef int (*nyxj_plugin_init_fn)(nyxj_app* app, const nyxj_host_api* api);
typedef void (*nyxj_plugin_shutdown_fn)(void);

/* The table handed to plugins (also usable by embedders). */
const nyxj_host_api* nyxj_host_api_get(nyxj_app* app);

#ifdef __cplusplus
}
#endif

#endif
