/*
 * WebAssembly bindings: the pure-C core (no threads, no network) compiled
 * with Emscripten so NyxJ runs in browsers, Node.js and edge workers.
 *
 * Every function takes UTF-8 strings and returns a heap string that the
 * caller releases with nyxj_wasm_free(). Results are JSON envelopes:
 *   {"ok": true, "result": ...}  or  {"ok": false, "error": {...}}
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_msgpack.h"
#include "../../include/nyxj_patch.h"
#include "../../include/nyxj_query.h"
#include "../../include/nyxj_repair.h"
#include "../../include/nyxj_schema.h"

#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT
#endif

static char* envelope(nyxj_value* result, const nyxj_error* err) {
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_bool(o, "ok", err == NULL);
    if (err) {
        nyxj_value* e = nyxj_new_object();
        nyxj_object_set_string(e, "code", nyxj_error_name(err->code));
        nyxj_object_set_string(e, "message", err->message);
        nyxj_object_set_number(e, "line", err->line);
        nyxj_object_set_number(e, "column", err->column);
        nyxj_object_set(o, "error", e);
        nyxj_free_value(result);
    } else {
        nyxj_object_set(o, "result", result ? result : nyxj_new_null());
    }
    char* out = nyxj_serialize(o);
    nyxj_free_value(o);
    return out;
}

EXPORT const char* nyxj_wasm_version(void) { return NYXJ_VERSION; }
EXPORT void nyxj_wasm_free(void* p) { free(p); }

EXPORT char* nyxj_wasm_format(const char* json, int indent) {
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) return envelope(NULL, &r.error);
    char* text = indent > 0 ? nyxj_serialize_pretty(r.root, indent) : nyxj_serialize(r.root);
    nyxj_free_value(r.root);
    nyxj_value* s = nyxj_new_string(text ? text : "");
    free(text);
    return envelope(s, NULL);
}

EXPORT char* nyxj_wasm_minify(const char* json) { return nyxj_wasm_format(json, 0); }

EXPORT char* nyxj_wasm_validate(const char* json) {
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) return envelope(NULL, &r.error);
    nyxj_value* info = nyxj_new_object();
    nyxj_object_set_string(info, "type", nyxj_type_name(r.root->type));
    nyxj_object_set_number(info, "size", nyxj_size(r.root));
    nyxj_free_value(r.root);
    return envelope(info, NULL);
}

EXPORT char* nyxj_wasm_query(const char* json, const char* path) {
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) return envelope(NULL, &r.error);
    nyxj_error err;
    nyxj_value* matches = nyxj_query(r.root, path, &err);
    nyxj_free_value(r.root);
    return matches ? envelope(matches, NULL) : envelope(NULL, &err);
}

EXPORT char* nyxj_wasm_repair(const char* text) {
    nyxj_repair_report rep;
    size_t n;
    char* fixed = nyxj_repair(text, strlen(text), &n, &rep);
    if (!fixed) {
        nyxj_error err;
        nyxj_error_clear(&err);
        nyxj_error_set(&err, NYXJ_ERR_INVALID_ARGUMENT, "no JSON value could be recovered");
        return envelope(NULL, &err);
    }
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "json", fixed);
    nyxj_object_set(o, "report", nyxj_repair_report_to_value(&rep));
    free(fixed);
    return envelope(o, NULL);
}

EXPORT char* nyxj_wasm_schema(const char* schema_json, const char* data_json) {
    nyxj_result s = nyxj_parse(schema_json);
    if (!s.is_valid) return envelope(NULL, &s.error);
    nyxj_result d = nyxj_parse(data_json);
    if (!d.is_valid) { nyxj_free_value(s.root); return envelope(NULL, &d.error); }
    nyxj_schema_report rep;
    bool ok = nyxj_schema_validate(s.root, d.root, &rep);
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_bool(o, "valid", ok);
    nyxj_object_set(o, "errors", nyxj_schema_report_to_value(&rep));
    nyxj_free_value(s.root);
    nyxj_free_value(d.root);
    return envelope(o, NULL);
}

EXPORT char* nyxj_wasm_diff(const char* from_json, const char* to_json) {
    nyxj_result a = nyxj_parse(from_json);
    if (!a.is_valid) return envelope(NULL, &a.error);
    nyxj_result b = nyxj_parse(to_json);
    if (!b.is_valid) { nyxj_free_value(a.root); return envelope(NULL, &b.error); }
    nyxj_value* patch = nyxj_diff(a.root, b.root);
    nyxj_free_value(a.root);
    nyxj_free_value(b.root);
    return envelope(patch, NULL);
}

EXPORT char* nyxj_wasm_msgpack_size(const char* json) {
    size_t n;
    nyxj_error err;
    uint8_t* bin = nyxj_json_to_msgpack(json, strlen(json), &n, &err);
    if (!bin) return envelope(NULL, &err);
    free(bin);
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_number(o, "json_bytes", (double)strlen(json));
    nyxj_object_set_number(o, "msgpack_bytes", (double)n);
    return envelope(o, NULL);
}
