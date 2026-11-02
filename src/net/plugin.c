#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../../include/nyxj_log.h"
#include "../core/internal.h"

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

/* --- Host function table --- */

static void host_free(void* p) { free(p); }

void nyxj_host_api_init(nyxj_app* app) {
    nyxj_host_api* a = &app->host_api;
    memset(a, 0, sizeof(*a));
    a->api_version = NYXJ_PLUGIN_API_VERSION;
    a->engine_version = NYXJ_VERSION;
    a->route = nyxj_app_route_ex;
    a->use = nyxj_app_use;
    a->req_param = nyxj_req_param;
    a->req_header = nyxj_req_header;
    a->req_query = nyxj_req_query;
    a->req_json = nyxj_req_json;
    a->res_json = nyxj_res_json;
    a->res_text = nyxj_res_text;
    a->res_error = nyxj_res_error;
    a->res_header = nyxj_res_header;
    a->parse = nyxj_parse_ex;
    a->serialize = nyxj_serialize;
    a->free_value = nyxj_free_value;
    a->new_object = nyxj_new_object;
    a->new_array = nyxj_new_array;
    a->new_string = nyxj_new_string;
    a->new_number = nyxj_new_number;
    a->new_bool = nyxj_new_bool;
    a->new_null = nyxj_new_null;
    a->object_set = nyxj_object_set;
    a->array_push = nyxj_array_push;
    a->get_path = nyxj_get_path;
    a->free_mem = host_free;
    a->log = nyxj_log_write;
    a->kv = app->kv;
    a->kv_set = nyxj_kv_set;
    a->kv_get = nyxj_kv_get;
    a->metrics = app->metrics;
    a->counter = nyxj_metrics_counter;
    a->metric_inc = nyxj_metric_inc;
}

const nyxj_host_api* nyxj_host_api_get(nyxj_app* app) { return app ? &app->host_api : NULL; }

/* --- Dynamic loading --- */

static void* lib_open(const char* path, char* why, size_t n) {
#if defined(_WIN32)
    HMODULE h = LoadLibraryA(path);
    if (!h) snprintf(why, n, "LoadLibrary failed (error %lu)", GetLastError());
    return (void*)h;
#else
    void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) snprintf(why, n, "%s", dlerror());
    return h;
#endif
}

static void* lib_sym(void* lib, const char* name) {
#if defined(_WIN32)
    return (void*)GetProcAddress((HMODULE)lib, name);
#else
    return dlsym(lib, name);
#endif
}

static void lib_close(void* lib) {
#if defined(_WIN32)
    FreeLibrary((HMODULE)lib);
#else
    dlclose(lib);
#endif
}

bool nyxj_app_load_plugin(nyxj_app* app, const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!app || !path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    char why[256] = "";
    void* lib = lib_open(path, why, sizeof(why));
    if (!lib) { nyxj_error_set(err, NYXJ_ERR_PLUGIN, "cannot load '%s': %s", path, why); return false; }

    nyxj_plugin_describe_fn describe;
    nyxj_plugin_init_fn init;
    nyxj_plugin_shutdown_fn shutdown;
    /* Object-to-function pointer conversion through memcpy keeps ISO C happy. */
    void* sym = lib_sym(lib, "nyxj_plugin_describe");
    memcpy(&describe, &sym, sizeof(sym));
    sym = lib_sym(lib, "nyxj_plugin_init");
    memcpy(&init, &sym, sizeof(sym));
    sym = lib_sym(lib, "nyxj_plugin_shutdown");
    memcpy(&shutdown, &sym, sizeof(sym));

    if (!describe || !init) {
        lib_close(lib);
        nyxj_error_set(err, NYXJ_ERR_PLUGIN, "'%s' does not export nyxj_plugin_describe/nyxj_plugin_init", path);
        return false;
    }
    const nyxj_plugin_meta* meta = describe();
    if (!meta || meta->api_version != NYXJ_PLUGIN_API_VERSION) {
        lib_close(lib);
        nyxj_error_set(err, NYXJ_ERR_PLUGIN, "'%s' targets plugin API %d, host provides %d", path,
                       meta ? meta->api_version : -1, NYXJ_PLUGIN_API_VERSION);
        return false;
    }
    int rc = init(app, &app->host_api);
    if (rc != 0) {
        lib_close(lib);
        nyxj_error_set(err, NYXJ_ERR_PLUGIN, "plugin '%s' failed to initialize (code %d)", meta->name, rc);
        return false;
    }
    nyxj_plugin_slot* g = realloc(app->plugins, sizeof(nyxj_plugin_slot) * (size_t)(app->nplugins + 1));
    if (!g) {
        if (shutdown) shutdown();
        lib_close(lib);
        nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL);
        return false;
    }
    app->plugins = g;
    nyxj_plugin_slot* slot = &app->plugins[app->nplugins++];
    slot->lib = lib;
    slot->path = nyxj_strdup(path);
    slot->meta = meta;
    slot->shutdown = shutdown;
    NYXJ_INFO("plugin", "loaded %s %s (%s)", meta->name ? meta->name : "?", meta->version ? meta->version : "?",
              meta->description ? meta->description : path);
    return true;
}

void nyxj_plugins_unload(nyxj_app* app) {
    for (int i = app->nplugins - 1; i >= 0; i--) {
        nyxj_plugin_slot* s = &app->plugins[i];
        if (s->shutdown) s->shutdown();
        lib_close(s->lib);
        free(s->path);
    }
    free(app->plugins);
    app->plugins = NULL;
    app->nplugins = 0;
}
