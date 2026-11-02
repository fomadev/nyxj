#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_kv.h"
#include "../common/thread.h"
#include "../core/internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

typedef struct entry {
    char* key;
    uint64_t hash;
    nyxj_value* value;
    uint64_t expires_at; /* Monotonic ms, 0 = never */
    struct entry* next;
} entry;

struct nyxj_kv {
    nyxj_mutex lock;
    entry** buckets;
    size_t nbuckets;
    size_t count;
    nyxj_kv_stats stats;
};

static uint64_t hash_key(const char* s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 0x100000001b3ULL;
    }
    return h;
}

static void entry_free(entry* e) {
    free(e->key);
    nyxj_free_value(e->value);
    free(e);
}

static bool expired(const entry* e, uint64_t now) { return e->expires_at && e->expires_at <= now; }

nyxj_kv* nyxj_kv_create(size_t initial_capacity) {
    nyxj_kv* kv = calloc(1, sizeof(*kv));
    if (!kv) return NULL;
    size_t n = 16;
    while (n < initial_capacity) n *= 2;
    kv->buckets = calloc(n, sizeof(entry*));
    if (!kv->buckets) { free(kv); return NULL; }
    kv->nbuckets = n;
    nyxj_mutex_init(&kv->lock);
    return kv;
}

static void clear_locked(nyxj_kv* kv) {
    for (size_t i = 0; i < kv->nbuckets; i++) {
        entry* e = kv->buckets[i];
        while (e) { entry* n = e->next; entry_free(e); e = n; }
        kv->buckets[i] = NULL;
    }
    kv->count = 0;
}

void nyxj_kv_destroy(nyxj_kv* kv) {
    if (!kv) return;
    clear_locked(kv);
    free(kv->buckets);
    nyxj_mutex_destroy(&kv->lock);
    free(kv);
}

static void grow_locked(nyxj_kv* kv) {
    size_t n = kv->nbuckets * 2;
    entry** nb = calloc(n, sizeof(entry*));
    if (!nb) return;
    for (size_t i = 0; i < kv->nbuckets; i++) {
        entry* e = kv->buckets[i];
        while (e) {
            entry* next = e->next;
            size_t b = (size_t)(e->hash & (n - 1));
            e->next = nb[b];
            nb[b] = e;
            e = next;
        }
    }
    free(kv->buckets);
    kv->buckets = nb;
    kv->nbuckets = n;
}

/* Finds a live entry; lazily evicts it when expired. */
static entry* find_locked(nyxj_kv* kv, const char* key, entry*** link_out) {
    uint64_t h = hash_key(key);
    entry** link = &kv->buckets[h & (kv->nbuckets - 1)];
    uint64_t now = nyxj_now_ms();
    while (*link) {
        entry* e = *link;
        if (e->hash == h && strcmp(e->key, key) == 0) {
            if (expired(e, now)) {
                *link = e->next;
                entry_free(e);
                kv->count--;
                kv->stats.expired++;
                return NULL;
            }
            if (link_out) *link_out = link;
            return e;
        }
        link = &e->next;
    }
    return NULL;
}

static bool set_locked(nyxj_kv* kv, const char* key, nyxj_value* owned, uint64_t ttl_ms) {
    entry* e = find_locked(kv, key, NULL);
    uint64_t exp = ttl_ms ? nyxj_now_ms() + ttl_ms : 0;
    kv->stats.sets++;
    if (e) {
        nyxj_free_value(e->value);
        e->value = owned;
        e->expires_at = exp;
        return true;
    }
    e = calloc(1, sizeof(entry));
    if (!e) return false;
    e->key = nyxj_strdup(key);
    if (!e->key) { free(e); return false; }
    e->hash = hash_key(key);
    e->value = owned;
    e->expires_at = exp;
    size_t b = (size_t)(e->hash & (kv->nbuckets - 1));
    e->next = kv->buckets[b];
    kv->buckets[b] = e;
    kv->count++;
    if (kv->count > kv->nbuckets * 3 / 4) grow_locked(kv);
    return true;
}

bool nyxj_kv_set(nyxj_kv* kv, const char* key, const nyxj_value* value, uint64_t ttl_ms) {
    if (!kv || !key || !value) return false;
    nyxj_value* copy = nyxj_clone(value);
    if (!copy) return false;
    nyxj_mutex_lock(&kv->lock);
    bool ok = set_locked(kv, key, copy, ttl_ms);
    nyxj_mutex_unlock(&kv->lock);
    if (!ok) nyxj_free_value(copy);
    return ok;
}

nyxj_value* nyxj_kv_get(nyxj_kv* kv, const char* key) {
    if (!kv || !key) return NULL;
    nyxj_mutex_lock(&kv->lock);
    entry* e = find_locked(kv, key, NULL);
    nyxj_value* out = e ? nyxj_clone(e->value) : NULL;
    if (e) kv->stats.hits++;
    else kv->stats.misses++;
    nyxj_mutex_unlock(&kv->lock);
    return out;
}

bool nyxj_kv_del(nyxj_kv* kv, const char* key) {
    if (!kv || !key) return false;
    nyxj_mutex_lock(&kv->lock);
    entry** link;
    entry* e = find_locked(kv, key, &link);
    if (e) {
        *link = e->next;
        entry_free(e);
        kv->count--;
    }
    nyxj_mutex_unlock(&kv->lock);
    return e != NULL;
}

bool nyxj_kv_exists(nyxj_kv* kv, const char* key) {
    if (!kv || !key) return false;
    nyxj_mutex_lock(&kv->lock);
    bool found = find_locked(kv, key, NULL) != NULL;
    nyxj_mutex_unlock(&kv->lock);
    return found;
}

bool nyxj_kv_incr(nyxj_kv* kv, const char* key, double delta, double* result) {
    if (!kv || !key) return false;
    bool ok = true;
    nyxj_mutex_lock(&kv->lock);
    entry* e = find_locked(kv, key, NULL);
    if (e) {
        if (e->value->type != NYXJ_NUMBER) ok = false;
        else {
            e->value->as.number += delta;
            if (result) *result = e->value->as.number;
        }
    } else {
        nyxj_value* v = nyxj_new_number(delta);
        ok = v && set_locked(kv, key, v, 0);
        if (!ok) nyxj_free_value(v);
        else if (result) *result = delta;
    }
    nyxj_mutex_unlock(&kv->lock);
    return ok;
}

bool nyxj_kv_expire(nyxj_kv* kv, const char* key, uint64_t ttl_ms) {
    if (!kv || !key) return false;
    nyxj_mutex_lock(&kv->lock);
    entry* e = find_locked(kv, key, NULL);
    if (e) e->expires_at = ttl_ms ? nyxj_now_ms() + ttl_ms : 0;
    nyxj_mutex_unlock(&kv->lock);
    return e != NULL;
}

int64_t nyxj_kv_ttl(nyxj_kv* kv, const char* key) {
    if (!kv || !key) return -2;
    nyxj_mutex_lock(&kv->lock);
    entry* e = find_locked(kv, key, NULL);
    int64_t out = -2;
    if (e && e->expires_at) {
        int64_t left = (int64_t)e->expires_at - (int64_t)nyxj_now_ms();
        out = left < 0 ? 0 : left;
    } else if (e) {
        out = -1;
    }
    nyxj_mutex_unlock(&kv->lock);
    return out;
}

size_t nyxj_kv_count(nyxj_kv* kv) {
    if (!kv) return 0;
    nyxj_mutex_lock(&kv->lock);
    size_t n = kv->count;
    nyxj_mutex_unlock(&kv->lock);
    return n;
}

static int cmp_str_values(const void* a, const void* b) {
    const nyxj_value* x = *(const nyxj_value* const*)a;
    const nyxj_value* y = *(const nyxj_value* const*)b;
    return strcmp(x->as.string, y->as.string);
}

nyxj_value* nyxj_kv_keys(nyxj_kv* kv, const char* prefix) {
    nyxj_value* arr = nyxj_new_array();
    if (!kv || !arr) return arr;
    size_t plen = prefix ? strlen(prefix) : 0;
    uint64_t now = nyxj_now_ms();
    nyxj_mutex_lock(&kv->lock);
    for (size_t i = 0; i < kv->nbuckets; i++)
        for (entry* e = kv->buckets[i]; e; e = e->next)
            if (!expired(e, now) && (plen == 0 || strncmp(e->key, prefix, plen) == 0))
                nyxj_array_push(arr, nyxj_new_string(e->key));
    nyxj_mutex_unlock(&kv->lock);
    if (arr->as.array.count > 1)
        qsort(arr->as.array.items, (size_t)arr->as.array.count, sizeof(nyxj_value*), cmp_str_values);
    return arr;
}

size_t nyxj_kv_sweep(nyxj_kv* kv) {
    if (!kv) return 0;
    size_t removed = 0;
    uint64_t now = nyxj_now_ms();
    nyxj_mutex_lock(&kv->lock);
    for (size_t i = 0; i < kv->nbuckets; i++) {
        entry** link = &kv->buckets[i];
        while (*link) {
            entry* e = *link;
            if (expired(e, now)) {
                *link = e->next;
                entry_free(e);
                kv->count--;
                removed++;
            } else {
                link = &e->next;
            }
        }
    }
    kv->stats.expired += removed;
    nyxj_mutex_unlock(&kv->lock);
    return removed;
}

void nyxj_kv_clear(nyxj_kv* kv) {
    if (!kv) return;
    nyxj_mutex_lock(&kv->lock);
    clear_locked(kv);
    nyxj_mutex_unlock(&kv->lock);
}

void nyxj_kv_stats_get(nyxj_kv* kv, nyxj_kv_stats* out) {
    if (!kv || !out) return;
    nyxj_mutex_lock(&kv->lock);
    *out = kv->stats;
    out->keys = kv->count;
    nyxj_mutex_unlock(&kv->lock);
}

bool nyxj_kv_save(nyxj_kv* kv, const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!kv || !path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    nyxj_value* snap = nyxj_new_object();
    if (!snap) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    uint64_t now = nyxj_now_ms();
    nyxj_mutex_lock(&kv->lock);
    for (size_t i = 0; i < kv->nbuckets; i++) {
        for (entry* e = kv->buckets[i]; e; e = e->next) {
            if (expired(e, now)) continue;
            nyxj_value* rec = nyxj_new_object();
            if (!rec) continue;
            nyxj_object_set(rec, "value", nyxj_clone(e->value));
            if (e->expires_at) nyxj_object_set_number(rec, "ttl_ms", (double)(e->expires_at - now));
            nyxj_object_set(snap, e->key, rec);
        }
    }
    nyxj_mutex_unlock(&kv->lock);
    char* text = nyxj_serialize(snap);
    nyxj_free_value(snap);
    if (!text) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    /* Write to a temporary file then rename: a crash never leaves a torn snapshot. */
    size_t plen = strlen(path);
    char* tmp = malloc(plen + 5);
    if (!tmp) { free(text); nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    memcpy(tmp, path, plen);
    memcpy(tmp + plen, ".tmp", 5);
    FILE* f = fopen(tmp, "wb");
    bool ok = f && fwrite(text, 1, strlen(text), f) == strlen(text);
    if (f && fclose(f) != 0) ok = false;
    free(text);
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    if (!ok) {
        nyxj_error_set(err, NYXJ_ERR_IO, "cannot write snapshot '%s': %s", path, strerror(errno));
        remove(tmp);
    }
    free(tmp);
    return ok;
}

bool nyxj_kv_load(nyxj_kv* kv, const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!kv || !path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    nyxj_value* snap = nyxj_parse_file(path, NULL, err);
    if (!snap) return false;
    if (snap->type != NYXJ_OBJECT) {
        nyxj_free_value(snap);
        nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "snapshot root must be an object");
        return false;
    }
    for (nyxj_pair* p = snap->as.object.pairs; p; p = p->next) {
        const nyxj_value* val = nyxj_get_member(p->value, "value");
        if (!val) continue;
        const nyxj_value* ttl = nyxj_get_member(p->value, "ttl_ms");
        uint64_t t = (ttl && ttl->type == NYXJ_NUMBER && ttl->as.number > 0) ? (uint64_t)ttl->as.number : 0;
        nyxj_kv_set(kv, p->key, val, t);
    }
    nyxj_free_value(snap);
    return true;
}
