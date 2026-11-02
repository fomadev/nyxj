/*
 * Embedded in-memory key-value store (a mini-Redis inside the engine).
 *
 * Thread-safe hash table of JSON values with per-key TTL, atomic counters,
 * prefix listing and JSON snapshots on disk for persistence. Keeps session
 * tokens, caches and configuration without any external database.
 */
#ifndef NYXJ_KV_H
#define NYXJ_KV_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nyxj_kv nyxj_kv;

typedef struct {
    size_t keys;
    uint64_t hits;
    uint64_t misses;
    uint64_t expired;
    uint64_t sets;
} nyxj_kv_stats;

nyxj_kv*    nyxj_kv_create(size_t initial_capacity);
void        nyxj_kv_destroy(nyxj_kv* kv);

bool        nyxj_kv_set(nyxj_kv* kv, const char* key, const nyxj_value* value, uint64_t ttl_ms); /* Copies; ttl 0 = forever */
nyxj_value* nyxj_kv_get(nyxj_kv* kv, const char* key); /* Copy (caller frees) or NULL */
bool        nyxj_kv_del(nyxj_kv* kv, const char* key);
bool        nyxj_kv_exists(nyxj_kv* kv, const char* key);
bool        nyxj_kv_incr(nyxj_kv* kv, const char* key, double delta, double* result); /* Creates at 0 */
bool        nyxj_kv_expire(nyxj_kv* kv, const char* key, uint64_t ttl_ms);
int64_t     nyxj_kv_ttl(nyxj_kv* kv, const char* key); /* ms left, -1 no TTL, -2 missing */
size_t      nyxj_kv_count(nyxj_kv* kv);
nyxj_value* nyxj_kv_keys(nyxj_kv* kv, const char* prefix); /* Array of key strings */
size_t      nyxj_kv_sweep(nyxj_kv* kv);                    /* Removes expired keys */
void        nyxj_kv_clear(nyxj_kv* kv);
void        nyxj_kv_stats_get(nyxj_kv* kv, nyxj_kv_stats* out);

/* Persistence: {"key": {"value": ..., "ttl_ms": N}, ...} */
bool nyxj_kv_save(nyxj_kv* kv, const char* path, nyxj_error* err);
bool nyxj_kv_load(nyxj_kv* kv, const char* path, nyxj_error* err);

#ifdef __cplusplus
}
#endif

#endif
