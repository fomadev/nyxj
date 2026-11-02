/*
 * nyxj-server: the NyxJ micro-service.
 *
 * Exposes the whole engine over HTTP(S): echo/format, schema validation,
 * JSONPath queries, repair, diff/patch, MessagePack, the embedded key-value
 * store, metrics, health probes, OpenAPI docs, Safe-Gate proxy, plugins and
 * gossip clustering. Run `nyxj-server --help` for the options.
 */
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj.h"
#include "../../include/nyxj_cluster.h"
#include "../../include/nyxj_http.h"
#include "../../include/nyxj_log.h"
#include "../../include/nyxj_msgpack.h"
#include "../../include/nyxj_patch.h"
#include "../../include/nyxj_query.h"
#include "../../include/nyxj_repair.h"
#include "../../include/nyxj_schema.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLUGINS 16
#define MAX_SEEDS 16

typedef struct {
    nyxj_app_config app;
    const char* plugins[MAX_PLUGINS];
    int nplugins;
    const char* gate_upstream;
    const char* cluster_bind;
    const char* cluster_advertise;
    const char* cluster_name;
    const char* seeds[MAX_SEEDS];
    int nseeds;
    const char* kv_snapshot;
    nyxj_value* config_doc; /* Keeps strings from --config alive */
} server_options;

static nyxj_app* g_app = NULL;
static nyxj_cluster* g_cluster = NULL;

static void on_signal(int sig) {
    (void)sig;
    nyxj_app_stop(g_app);
}

/* --- Helpers --- */

static nyxj_value* body_object(nyxj_request* req, nyxj_response* res) {
    nyxj_value* doc = nyxj_req_json(req);
    if (!doc) {
        nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message);
        return NULL;
    }
    if (doc->type != NYXJ_OBJECT) {
        nyxj_res_error(res, 400, "invalid_request", "request body must be a JSON object");
        return NULL;
    }
    return doc;
}

static const nyxj_value* require_member(const nyxj_value* obj, const char* key, nyxj_response* res) {
    const nyxj_value* v = nyxj_get_member(obj, key);
    if (!v) {
        char msg[96];
        snprintf(msg, sizeof(msg), "missing '%s' member", key);
        nyxj_res_error(res, 400, "invalid_request", msg);
    }
    return v;
}

static int query_int(nyxj_request* req, const char* name, int fallback) {
    char buf[32];
    if (!nyxj_req_query(req, name, buf, sizeof(buf)) || !*buf) return fallback;
    return atoi(buf);
}

/* --- JSON tools --- */

static void h_data(nyxj_request* req, nyxj_response* res) {
    nyxj_value* doc = nyxj_req_json(req);
    if (!doc) {
        nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message);
        return;
    }
    nyxj_res_json(res, 200, nyxj_clone(doc));
}

static void h_format(nyxj_request* req, nyxj_response* res) {
    nyxj_value* doc = nyxj_req_json(req);
    if (!doc) { nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message); return; }
    nyxj_serialize_options o = {query_int(req, "indent", 2), query_int(req, "sort", 0) != 0, false};
    size_t n;
    char* text = nyxj_serialize_ex(doc, &o, &n);
    nyxj_res_bytes(res, 200, "application/json", text, n);
    free(text);
}

static void h_validate(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = body_object(req, res);
    if (!body) return;
    const nyxj_value* schema = require_member(body, "schema", res);
    const nyxj_value* data = schema ? require_member(body, "data", res) : NULL;
    if (!data) return;
    nyxj_schema_report rep;
    bool ok = nyxj_schema_validate(schema, data, &rep);
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_bool(o, "valid", ok);
    nyxj_object_set_number(o, "error_count", rep.total);
    nyxj_object_set(o, "errors", nyxj_schema_report_to_value(&rep));
    nyxj_res_json(res, ok ? 200 : 422, o);
}

static void h_query(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = body_object(req, res);
    if (!body) return;
    const char* q = nyxj_get_string(body, "query");
    const nyxj_value* data = nyxj_get_member(body, "data");
    if (!q || !data) { nyxj_res_error(res, 400, "invalid_request", "expected {\"query\": \"$...\", \"data\": ...}"); return; }
    nyxj_error err;
    nyxj_value* matches = nyxj_query(data, q, &err);
    if (!matches) { nyxj_res_error(res, 400, "invalid_query", err.message); return; }
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_number(o, "count", matches->as.array.count);
    nyxj_object_set(o, "matches", matches);
    nyxj_res_json(res, 200, o);
}

static void h_repair(nyxj_request* req, nyxj_response* res) {
    nyxj_repair_report rep;
    size_t n;
    char* fixed = nyxj_repair(req->body, req->body_len, &n, &rep);
    if (!fixed) { nyxj_res_error(res, 422, "unrepairable", "no JSON value could be recovered"); return; }
    nyxj_result r = nyxj_parse_n(fixed, n);
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_bool(o, "valid", r.is_valid);
    nyxj_object_set_string(o, "repaired", fixed);
    nyxj_object_set(o, "report", nyxj_repair_report_to_value(&rep));
    if (r.is_valid) nyxj_object_set(o, "result", r.root);
    nyxj_res_json(res, 200, o);
    free(fixed);
}

static void h_diff(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = body_object(req, res);
    if (!body) return;
    const nyxj_value* from = require_member(body, "from", res);
    const nyxj_value* to = from ? require_member(body, "to", res) : NULL;
    if (!to) return;
    char mode[16] = "";
    nyxj_req_query(req, "mode", mode, sizeof(mode));
    nyxj_res_json(res, 200, strcmp(mode, "merge") == 0 ? nyxj_merge_diff(from, to) : nyxj_diff(from, to));
}

static void h_patch(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = body_object(req, res);
    if (!body) return;
    const nyxj_value* doc = require_member(body, "document", res);
    const nyxj_value* patch = doc ? require_member(body, "patch", res) : NULL;
    if (!patch) return;
    nyxj_value* out = nyxj_clone(doc);
    nyxj_error err;
    if (patch->type == NYXJ_OBJECT) { /* Merge patch (RFC 7386) */
        nyxj_res_json(res, 200, nyxj_merge_patch(out, patch));
        return;
    }
    if (!nyxj_patch_apply(&out, patch, &err)) {
        nyxj_free_value(out);
        nyxj_res_error(res, err.code == NYXJ_ERR_TEST_FAILED ? 409 : 422, nyxj_error_name(err.code), err.message);
        return;
    }
    nyxj_res_json(res, 200, out);
}

static void h_msgpack_encode(nyxj_request* req, nyxj_response* res) {
    nyxj_value* doc = nyxj_req_json(req);
    if (!doc) { nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message); return; }
    size_t n;
    uint8_t* bin = nyxj_msgpack_encode(doc, &n);
    char ratio[32];
    snprintf(ratio, sizeof(ratio), "%.3f", req->body_len ? (double)n / (double)req->body_len : 0.0);
    nyxj_res_bytes(res, 200, "application/msgpack", bin, n);
    nyxj_res_header(res, "X-NyxJ-Size-Ratio", ratio);
    free(bin);
}

static void h_msgpack_decode(nyxj_request* req, nyxj_response* res) {
    nyxj_error err;
    nyxj_value* v = nyxj_msgpack_decode((const uint8_t*)req->body, req->body_len, &err);
    if (!v) { nyxj_res_error(res, 400, "invalid_msgpack", err.message); return; }
    nyxj_res_json(res, 200, v);
}

/* --- Key-value store --- */

static void h_kv_list(nyxj_request* req, nyxj_response* res) {
    char prefix[128] = "";
    nyxj_req_query(req, "prefix", prefix, sizeof(prefix));
    nyxj_value* keys = nyxj_kv_keys(nyxj_app_kv(req->app), prefix);
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_number(o, "count", keys ? keys->as.array.count : 0);
    nyxj_object_set(o, "keys", keys);
    nyxj_res_json(res, 200, o);
}

static void h_kv_get(nyxj_request* req, nyxj_response* res) {
    const char* key = nyxj_req_param(req, "key");
    nyxj_value* v = nyxj_kv_get(nyxj_app_kv(req->app), key);
    if (!v) { nyxj_res_error(res, 404, "not_found", "no such key"); return; }
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "key", key);
    nyxj_object_set(o, "value", v);
    nyxj_object_set_number(o, "ttl_ms", (double)nyxj_kv_ttl(nyxj_app_kv(req->app), key));
    nyxj_res_json(res, 200, o);
}

static void h_kv_put(nyxj_request* req, nyxj_response* res) {
    nyxj_value* v = nyxj_req_json(req);
    if (!v) { nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message); return; }
    int ttl = query_int(req, "ttl", 0);
    const char* key = nyxj_req_param(req, "key");
    if (!nyxj_kv_set(nyxj_app_kv(req->app), key, v, ttl > 0 ? (uint64_t)ttl : 0)) {
        nyxj_res_error(res, 500, "store_error", "cannot store the value");
        return;
    }
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "key", key);
    nyxj_object_set_bool(o, "stored", true);
    nyxj_res_json(res, 201, o);
}

static void h_kv_delete(nyxj_request* req, nyxj_response* res) {
    bool deleted = nyxj_kv_del(nyxj_app_kv(req->app), nyxj_req_param(req, "key"));
    if (!deleted) { nyxj_res_error(res, 404, "not_found", "no such key"); return; }
    res->status = 204;
}

static void h_kv_incr(nyxj_request* req, nyxj_response* res) {
    char by[32] = "1";
    nyxj_req_query(req, "by", by, sizeof(by));
    double result;
    const char* key = nyxj_req_param(req, "key");
    if (!nyxj_kv_incr(nyxj_app_kv(req->app), key, atof(by), &result)) {
        nyxj_res_error(res, 409, "type_mismatch", "the stored value is not a number");
        return;
    }
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "key", key);
    nyxj_object_set_number(o, "value", result);
    nyxj_res_json(res, 200, o);
}

/* --- Information --- */

static void h_info(nyxj_request* req, nyxj_response* res) {
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "engine", "NyxJ");
    nyxj_object_set_string(o, "version", nyxj_version());
    nyxj_object_set_string(o, "simd", nyxj_simd_backend());
    nyxj_object_set_string(o, "features", nyxj_build_features());
    nyxj_object_set_number(o, "uptime_s", nyxj_app_uptime(req->app));
    nyxj_pool* p = nyxj_app_pool(req->app);
    if (p) {
        nyxj_pool_stats ps;
        nyxj_pool_stats_get(p, &ps);
        nyxj_object_set_number(o, "worker_threads", ps.threads);
    }
    nyxj_object_set_number(o, "eco_level", nyxj_app_eco_level(req->app));
    if (g_cluster) nyxj_object_set_string(o, "node_id", nyxj_cluster_node_id(g_cluster));
    nyxj_res_json(res, 200, o);
}

static void h_cluster(nyxj_request* req, nyxj_response* res) {
    (void)req;
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "node_id", nyxj_cluster_node_id(g_cluster));
    nyxj_object_set_number(o, "alive", nyxj_cluster_alive_count(g_cluster));
    char best[128];
    if (nyxj_cluster_least_loaded(g_cluster, best, sizeof(best))) nyxj_object_set_string(o, "least_loaded_peer", best);
    nyxj_object_set(o, "members", nyxj_cluster_members(g_cluster));
    nyxj_res_json(res, 200, o);
}

static void register_routes(nyxj_app* app) {
    nyxj_route_doc d;
    memset(&d, 0, sizeof(d));
#define ROUTE(m, p, h, fl, tag_, sum, rq, rs)                         \
    do {                                                               \
        d.tag = tag_; d.summary = sum; d.request_schema = rq; d.response_schema = rs; \
        nyxj_app_route_ex(app, m, p, h, NULL, fl, &d);                 \
    } while (0)
    const unsigned A = NYXJ_ROUTE_ASYNC;
    ROUTE("POST", "/api/data", h_data, A, "json", "Parse and echo a JSON document (1.x compatible)", NULL, NULL);
    ROUTE("POST", "/api/format", h_format, A, "json", "Pretty-print (?indent=N&sort=1)", NULL, NULL);
    ROUTE("POST", "/api/validate", h_validate, A, "json", "Validate data against a JSON Schema",
          "{\"type\":\"object\",\"required\":[\"schema\",\"data\"],\"properties\":{\"schema\":{\"type\":\"object\"},\"data\":{}}}",
          "{\"type\":\"object\",\"properties\":{\"valid\":{\"type\":\"boolean\"},\"errors\":{\"type\":\"array\"}}}");
    ROUTE("POST", "/api/query", h_query, A, "json", "Run a JSONPath (NyxQuery) expression",
          "{\"type\":\"object\",\"required\":[\"query\",\"data\"],\"properties\":{\"query\":{\"type\":\"string\"},\"data\":{}}}",
          "{\"type\":\"object\",\"properties\":{\"count\":{\"type\":\"integer\"},\"matches\":{\"type\":\"array\"}}}");
    ROUTE("POST", "/api/repair", h_repair, A, "json", "Repair almost-JSON text (fuzzy parsing)", NULL, NULL);
    ROUTE("POST", "/api/diff", h_diff, A, "json", "JSON Patch between two documents (?mode=merge for RFC 7386)",
          "{\"type\":\"object\",\"required\":[\"from\",\"to\"]}", NULL);
    ROUTE("POST", "/api/patch", h_patch, A, "json", "Apply a JSON Patch (array) or a Merge Patch (object)",
          "{\"type\":\"object\",\"required\":[\"document\",\"patch\"]}", NULL);
    ROUTE("POST", "/api/msgpack/encode", h_msgpack_encode, A, "binary", "Convert JSON to MessagePack", NULL, NULL);
    ROUTE("POST", "/api/msgpack/decode", h_msgpack_decode, A, "binary", "Convert MessagePack to JSON", NULL, NULL);
    ROUTE("GET", "/api/kv", h_kv_list, 0, "store", "List keys (?prefix=)", NULL, NULL);
    ROUTE("GET", "/api/kv/:key", h_kv_get, 0, "store", "Read a key", NULL, NULL);
    ROUTE("PUT", "/api/kv/:key", h_kv_put, 0, "store", "Store a JSON value (?ttl=ms)", NULL, NULL);
    ROUTE("DELETE", "/api/kv/:key", h_kv_delete, 0, "store", "Delete a key", NULL, NULL);
    ROUTE("POST", "/api/kv/:key/incr", h_kv_incr, 0, "store", "Atomically increment a number (?by=N)", NULL, NULL);
    ROUTE("GET", "/api/info", h_info, 0, "operations", "Engine version, SIMD backend and features", NULL, NULL);
#undef ROUTE
}

/* --- Command line --- */

static void usage(void) {
    printf("NyxJ server %s\n\n"
           "Usage: nyxj-server [options]\n\n"
           "  --listen URL           Listen address (default http://0.0.0.0:8000)\n"
           "  --port N               Shortcut for --listen http://0.0.0.0:N\n"
           "  --threads N            Worker threads (0 = one per CPU, -1 = none)\n"
           "  --queue N              Pending async jobs before 503 (default 1024)\n"
           "  --max-body BYTES       Largest accepted body (default 16777216)\n"
           "  --max-depth N          JSON nesting limit (default 512)\n"
           "  --tls-cert FILE        PEM certificate (enables HTTPS)\n"
           "  --tls-key FILE         PEM private key\n"
           "  --log-level LEVEL      trace|debug|info|warn|error|off (default info)\n"
           "  --log-json             Structured JSON logs\n"
           "  --no-access-log        Disable per-request logging\n"
           "  --no-cors              Disable CORS headers\n"
           "  --plugin FILE          Load a plugin (.so/.dll), repeatable\n"
           "  --gate-upstream URL    Safe-Gate: sanitize and forward unmatched requests\n"
           "  --kv-snapshot FILE     Load the key-value store at start, save it on exit\n"
           "  --heartbeat-ms N       Resource sampling period (default 5000)\n"
           "  --heartbeat-url URL    POST a heartbeat to this URL\n"
           "  --eco-temp C           Eco-Throttle above this CPU temperature\n"
           "  --eco-load L           Eco-Throttle above this load per CPU\n"
           "  --cluster-bind URL     Enable gossip clustering, e.g. udp://0.0.0.0:7946\n"
           "  --cluster-advertise U  Gossip address announced to peers\n"
           "  --cluster-name NAME    Node name\n"
           "  --seed URL             Gossip seed, repeatable\n"
           "  --config FILE          JSON configuration file (keys mirror the options)\n"
           "  --version, --help\n",
           NYXJ_VERSION);
}

static const char* cfg_str(const nyxj_value* cfg, const char* key, const char* fallback) {
    const char* s = nyxj_get_string(cfg, key);
    return s ? s : fallback;
}

static bool load_config(const char* path, server_options* o) {
    nyxj_error err;
    nyxj_parse_options po = nyxj_parse_options_default();
    po.flags = NYXJ_PARSE_RELAXED; /* Comments and trailing commas are welcome in config files */
    nyxj_value* cfg = nyxj_parse_file(path, &po, &err);
    if (!cfg) { fprintf(stderr, "config: %s\n", err.message); return false; }
    o->config_doc = cfg;
    nyxj_app_config* a = &o->app;
    a->listen_url = cfg_str(cfg, "listen", a->listen_url);
    if (nyxj_get_member(cfg, "threads")) a->threads = (int)nyxj_get_int(cfg, "threads");
    if (nyxj_get_member(cfg, "queue")) a->queue_capacity = (int)nyxj_get_int(cfg, "queue");
    if (nyxj_get_member(cfg, "max_body")) a->max_body_size = (size_t)nyxj_get_int(cfg, "max_body");
    if (nyxj_get_member(cfg, "max_depth")) a->parse.max_depth = (int)nyxj_get_int(cfg, "max_depth");
    a->tls_cert_file = cfg_str(cfg, "tls_cert", a->tls_cert_file);
    a->tls_key_file = cfg_str(cfg, "tls_key", a->tls_key_file);
    a->title = cfg_str(cfg, "title", a->title);
    a->heartbeat_url = cfg_str(cfg, "heartbeat_url", a->heartbeat_url);
    if (nyxj_get_member(cfg, "heartbeat_ms")) a->heartbeat_ms = (int)nyxj_get_int(cfg, "heartbeat_ms");
    if (nyxj_get_member(cfg, "eco_temp")) a->eco_temp_c = nyxj_get_number(cfg, "eco_temp");
    if (nyxj_get_member(cfg, "eco_load")) a->eco_load = nyxj_get_number(cfg, "eco_load");
    if (nyxj_get_member(cfg, "cors")) a->cors = nyxj_get_bool(cfg, "cors");
    if (nyxj_get_member(cfg, "access_log")) a->access_log = nyxj_get_bool(cfg, "access_log");
    const char* level = nyxj_get_string(cfg, "log_level");
    nyxj_log_level lv;
    if (level && nyxj_log_level_parse(level, &lv)) nyxj_log_set_level(lv);
    if (nyxj_get_bool(cfg, "log_json")) nyxj_log_set_json(true);
    o->gate_upstream = cfg_str(cfg, "gate_upstream", o->gate_upstream);
    o->kv_snapshot = cfg_str(cfg, "kv_snapshot", o->kv_snapshot);
    o->cluster_bind = cfg_str(cfg, "cluster_bind", o->cluster_bind);
    o->cluster_advertise = cfg_str(cfg, "cluster_advertise", o->cluster_advertise);
    o->cluster_name = cfg_str(cfg, "cluster_name", o->cluster_name);
    const nyxj_value* plugins = nyxj_get_member(cfg, "plugins");
    for (int i = 0; plugins && plugins->type == NYXJ_ARRAY && i < plugins->as.array.count && o->nplugins < MAX_PLUGINS; i++)
        if (plugins->as.array.items[i]->type == NYXJ_STRING) o->plugins[o->nplugins++] = plugins->as.array.items[i]->as.string;
    const nyxj_value* seeds = nyxj_get_member(cfg, "seeds");
    for (int i = 0; seeds && seeds->type == NYXJ_ARRAY && i < seeds->as.array.count && o->nseeds < MAX_SEEDS; i++)
        if (seeds->as.array.items[i]->type == NYXJ_STRING) o->seeds[o->nseeds++] = seeds->as.array.items[i]->as.string;
    return true;
}

static int parse_args(int argc, char** argv, server_options* o, char* listen_buf, size_t listen_n) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : NULL;
#define NEED_VALUE() do { if (!v) { fprintf(stderr, "%s requires a value\n", a); return 2; } i++; } while (0)
        if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 1; }
        else if (!strcmp(a, "--version") || !strcmp(a, "-v")) { printf("nyxj-server %s\n", NYXJ_VERSION); return 1; }
        else if (!strcmp(a, "--config")) { NEED_VALUE(); if (!load_config(v, o)) return 2; }
        else if (!strcmp(a, "--listen")) { NEED_VALUE(); o->app.listen_url = v; }
        else if (!strcmp(a, "--port")) { NEED_VALUE(); snprintf(listen_buf, listen_n, "http://0.0.0.0:%s", v); o->app.listen_url = listen_buf; }
        else if (!strcmp(a, "--threads")) { NEED_VALUE(); o->app.threads = atoi(v); }
        else if (!strcmp(a, "--queue")) { NEED_VALUE(); o->app.queue_capacity = atoi(v); }
        else if (!strcmp(a, "--max-body")) { NEED_VALUE(); o->app.max_body_size = (size_t)strtoull(v, NULL, 10); }
        else if (!strcmp(a, "--max-depth")) { NEED_VALUE(); o->app.parse.max_depth = atoi(v); }
        else if (!strcmp(a, "--tls-cert")) { NEED_VALUE(); o->app.tls_cert_file = v; }
        else if (!strcmp(a, "--tls-key")) { NEED_VALUE(); o->app.tls_key_file = v; }
        else if (!strcmp(a, "--log-level")) {
            NEED_VALUE();
            nyxj_log_level lv;
            if (!nyxj_log_level_parse(v, &lv)) { fprintf(stderr, "unknown log level '%s'\n", v); return 2; }
            nyxj_log_set_level(lv);
        }
        else if (!strcmp(a, "--log-json")) nyxj_log_set_json(true);
        else if (!strcmp(a, "--no-access-log")) o->app.access_log = false;
        else if (!strcmp(a, "--no-cors")) o->app.cors = false;
        else if (!strcmp(a, "--plugin")) { NEED_VALUE(); if (o->nplugins < MAX_PLUGINS) o->plugins[o->nplugins++] = v; }
        else if (!strcmp(a, "--gate-upstream")) { NEED_VALUE(); o->gate_upstream = v; }
        else if (!strcmp(a, "--kv-snapshot")) { NEED_VALUE(); o->kv_snapshot = v; }
        else if (!strcmp(a, "--heartbeat-ms")) { NEED_VALUE(); o->app.heartbeat_ms = atoi(v); }
        else if (!strcmp(a, "--heartbeat-url")) { NEED_VALUE(); o->app.heartbeat_url = v; }
        else if (!strcmp(a, "--eco-temp")) { NEED_VALUE(); o->app.eco_temp_c = atof(v); }
        else if (!strcmp(a, "--eco-load")) { NEED_VALUE(); o->app.eco_load = atof(v); }
        else if (!strcmp(a, "--cluster-bind")) { NEED_VALUE(); o->cluster_bind = v; }
        else if (!strcmp(a, "--cluster-advertise")) { NEED_VALUE(); o->cluster_advertise = v; }
        else if (!strcmp(a, "--cluster-name")) { NEED_VALUE(); o->cluster_name = v; }
        else if (!strcmp(a, "--seed")) { NEED_VALUE(); if (o->nseeds < MAX_SEEDS) o->seeds[o->nseeds++] = v; }
        else { fprintf(stderr, "unknown option '%s' (see --help)\n", a); return 2; }
#undef NEED_VALUE
    }
    return 0;
}

int main(int argc, char** argv) {
    server_options o;
    memset(&o, 0, sizeof(o));
    o.app = nyxj_app_config_default();
    o.app.title = "NyxJ Server";
    char listen_buf[96];
    int rc = parse_args(argc, argv, &o, listen_buf, sizeof(listen_buf));
    if (rc) {
        nyxj_free_value(o.config_doc);
        return rc == 1 ? 0 : rc;
    }

    printf("NyxJ Micro-service v%s (SIMD: %s)\n", NYXJ_VERSION, nyxj_simd_backend());
    g_app = nyxj_app_create(&o.app);
    if (!g_app) { fprintf(stderr, "cannot create the application\n"); return 1; }
    register_routes(g_app);

    nyxj_error err;
    for (int i = 0; i < o.nplugins; i++)
        if (!nyxj_app_load_plugin(g_app, o.plugins[i], &err)) NYXJ_ERROR("server", "%s", err.message);
    if (o.gate_upstream) nyxj_app_enable_gate_proxy(g_app, o.gate_upstream, NULL);
    if (o.kv_snapshot && !nyxj_kv_load(nyxj_app_kv(g_app), o.kv_snapshot, &err))
        NYXJ_INFO("server", "no snapshot loaded (%s)", err.message);

    if (!nyxj_app_listen(g_app, &err)) {
        NYXJ_FATAL("server", "%s", err.message);
        nyxj_app_destroy(g_app);
        nyxj_free_value(o.config_doc);
        return 1;
    }
    if (o.cluster_bind) {
        nyxj_cluster_config cc;
        memset(&cc, 0, sizeof(cc));
        cc.bind_url = o.cluster_bind;
        cc.advertise_addr = o.cluster_advertise;
        cc.node_name = o.cluster_name;
        cc.http_addr = o.app.listen_url;
        cc.seeds = o.seeds;
        cc.seed_count = o.nseeds;
        g_cluster = nyxj_cluster_start(g_app, &cc, &err);
        if (g_cluster) nyxj_app_get(g_app, "/cluster/members", h_cluster, NULL);
        else NYXJ_ERROR("server", "%s", err.message);
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    printf("Server active at: %s (docs: /docs, metrics: /metrics)\nPress Ctrl+C to stop.\n", o.app.listen_url);
    fflush(stdout);

    rc = nyxj_app_run(g_app);
    if (o.kv_snapshot && !nyxj_kv_save(nyxj_app_kv(g_app), o.kv_snapshot, &err)) NYXJ_ERROR("server", "%s", err.message);
    nyxj_app_destroy(g_app);
    nyxj_free_value(o.config_doc);
    return rc;
}
