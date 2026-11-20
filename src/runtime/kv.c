#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_kv.h"
#include "../common/thread.h"
#include "../core/internal.h"
#include "../../include/nyxj_log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define nyxj_fsync(f) _commit(_fileno(f))
#else
#include <unistd.h>
#define nyxj_fsync(f) fsync(fileno(f))
#endif

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
    FILE* wal;        /* Write-ahead log, NULL when disabled */
    char* wal_path;
    bool wal_sync;
    size_t wal_replayed;
};

/* --- Write-ahead log records --- */

/* Appends one record; called with the lock held, after the mutation succeeded. */
static void wal_log(nyxj_kv* kv, const char* op, const char* key, const nyxj_value* value, uint64_t expires_at) {
    if (!kv->wal) return;
    nyxj_value* rec = nyxj_new_object();
    if (!rec) return;
    nyxj_object_set_string(rec, "op", op);
    if (key) nyxj_object_set_string(rec, "k", key);
    if (value) nyxj_object_set(rec, "v", nyxj_clone(value));
    if (expires_at) { /* Monotonic deadline -> wall clock, so replay works after a restart */
        uint64_t now = nyxj_now_ms();
        uint64_t left = expires_at > now ? expires_at - now : 0;
        nyxj_object_set_number(rec, "exp", (double)(nyxj_wall_ms() + left));
    }
    size_t len = 0;
    char* line = nyxj_serialize_ex(rec, NULL, &len);
    nyxj_free_value(rec);
    if (!line) return;
    fwrite(line, 1, len, kv->wal);
    fputc('\n', kv->wal);
    fflush(kv->wal);
    if (kv->wal_sync) nyxj_fsync(kv->wal);
    free(line);
}

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
    nyxj_kv_wal_close(kv);
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
    if (ok && kv->wal) {
        entry* e = find_locked(kv, key, NULL);
        if (e) wal_log(kv, "set", key, e->value, e->expires_at);
    }
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
        wal_log(kv, "del", key, NULL, 0);
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
    if (ok && kv->wal) { /* Logged as the resulting value: replay stays idempotent */
        entry* cur = find_locked(kv, key, NULL);
        if (cur) wal_log(kv, "set", key, cur->value, cur->expires_at);
    }
    nyxj_mutex_unlock(&kv->lock);
    return ok;
}

bool nyxj_kv_expire(nyxj_kv* kv, const char* key, uint64_t ttl_ms) {
    if (!kv || !key) return false;
    nyxj_mutex_lock(&kv->lock);
    entry* e = find_locked(kv, key, NULL);
    if (e) {
        e->expires_at = ttl_ms ? nyxj_now_ms() + ttl_ms : 0;
        wal_log(kv, "set", key, e->value, e->expires_at);
    }
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
    wal_log(kv, "clear", NULL, NULL, 0);
    nyxj_mutex_unlock(&kv->lock);
}

void nyxj_kv_stats_get(nyxj_kv* kv, nyxj_kv_stats* out) {
    if (!kv || !out) return;
    nyxj_mutex_lock(&kv->lock);
    *out = kv->stats;
    out->keys = kv->count;
    nyxj_mutex_unlock(&kv->lock);
}

/* Serializes the live entries; called with the lock held. */
static char* snapshot_locked(nyxj_kv* kv) {
    nyxj_value* snap = nyxj_new_object();
    if (!snap) return NULL;
    uint64_t now = nyxj_now_ms();
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
    char* text = nyxj_serialize(snap);
    nyxj_free_value(snap);
    return text;
}

/* Writes to a temporary file then renames it: a crash never leaves a torn file. */
static bool write_atomic(const char* path, const char* text, size_t len, nyxj_error* err) {
    size_t plen = strlen(path);
    char* tmp = malloc(plen + 5);
    if (!tmp) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    memcpy(tmp, path, plen);
    memcpy(tmp + plen, ".tmp", 5);
    FILE* f = fopen(tmp, "wb");
    bool ok = f && fwrite(text, 1, len, f) == len;
    if (f && fflush(f) != 0) ok = false;
    if (f && ok) nyxj_fsync(f);
    if (f && fclose(f) != 0) ok = false;
    if (ok) {
        remove(path);
        ok = rename(tmp, path) == 0;
    }
    if (!ok) {
        nyxj_error_set(err, NYXJ_ERR_IO, "cannot write '%s': %s", path, strerror(errno));
        remove(tmp);
    }
    free(tmp);
    return ok;
}

bool nyxj_kv_save(nyxj_kv* kv, const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!kv || !path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    nyxj_mutex_lock(&kv->lock);
    char* text = snapshot_locked(kv);
    nyxj_mutex_unlock(&kv->lock);
    if (!text) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    bool ok = write_atomic(path, text, strlen(text), err);
    free(text);
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

/* --- Write-ahead log --- */

/* Applies one WAL record (lock held, WAL detached). Returns false on a malformed record. */
static bool wal_apply(nyxj_kv* kv, const nyxj_value* rec) {
    const char* op = nyxj_get_string(rec, "op");
    const char* key = nyxj_get_string(rec, "k");
    if (!op) return false;
    if (strcmp(op, "clear") == 0) {
        clear_locked(kv);
        return true;
    }
    if (!key) return false;
    if (strcmp(op, "del") == 0) {
        entry** link;
        entry* e = find_locked(kv, key, &link);
        if (e) {
            *link = e->next;
            entry_free(e);
            kv->count--;
        }
        return true;
    }
    if (strcmp(op, "set") == 0) {
        const nyxj_value* v = nyxj_get_member(rec, "v");
        if (!v) return false;
        uint64_t ttl = 0;
        const nyxj_value* exp = nyxj_get_member(rec, "exp");
        if (exp && exp->type == NYXJ_NUMBER) {
            double left = exp->as.number - (double)nyxj_wall_ms();
            if (left <= 0) { /* Already expired: make sure no older value survives */
                entry** link;
                entry* e = find_locked(kv, key, &link);
                if (e) {
                    *link = e->next;
                    entry_free(e);
                    kv->count--;
                }
                return true;
            }
            ttl = (uint64_t)left;
        }
        nyxj_value* copy = nyxj_clone(v);
        if (!copy) return false;
        if (!set_locked(kv, key, copy, ttl)) { nyxj_free_value(copy); return false; }
        kv->stats.sets--; /* Replay is not client traffic */
        return true;
    }
    return false;
}

bool nyxj_kv_wal_open(nyxj_kv* kv, const char* path, bool sync, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!kv || !path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    nyxj_kv_wal_close(kv);

    size_t len = 0, good = 0, applied = 0;
    nyxj_error rerr;
    char* data = nyxj_read_file(path, &len, &rerr); /* Missing file: fresh log */
    nyxj_mutex_lock(&kv->lock);
    if (data) {
        size_t pos = 0;
        while (pos < len) {
            const char* nl = memchr(data + pos, '\n', len - pos);
            if (!nl) break; /* Torn last record: no newline */
            size_t n = (size_t)(nl - (data + pos));
            if (n > 0) {
                nyxj_result r = nyxj_parse_n(data + pos, n);
                bool ok = r.is_valid && r.root->type == NYXJ_OBJECT && wal_apply(kv, r.root);
                nyxj_free_value(r.root);
                if (!ok) break;
                applied++;
            }
            pos += n + 1;
            good = pos;
        }
    }
    kv->wal_replayed = applied;
    nyxj_mutex_unlock(&kv->lock);

    bool ok = true;
    if (data && good < len) { /* Drop the damaged tail so new records start on a clean line */
        ok = write_atomic(path, data, good, err);
        if (ok) NYXJ_WARN("kv", "WAL %s: discarded %lu damaged trailing bytes", path, (unsigned long)(len - good));
    }
    free(data);
    if (!ok) return false;

    FILE* f = fopen(path, "ab");
    if (!f) {
        nyxj_error_set(err, NYXJ_ERR_IO, "cannot open WAL '%s': %s", path, strerror(errno));
        return false;
    }
    char* p = nyxj_strdup(path);
    if (!p) { fclose(f); nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    nyxj_mutex_lock(&kv->lock);
    kv->wal = f;
    kv->wal_path = p;
    kv->wal_sync = sync;
    nyxj_mutex_unlock(&kv->lock);
    return true;
}

void nyxj_kv_wal_close(nyxj_kv* kv) {
    if (!kv) return;
    nyxj_mutex_lock(&kv->lock);
    if (kv->wal) fclose(kv->wal);
    kv->wal = NULL;
    free(kv->wal_path);
    kv->wal_path = NULL;
    nyxj_mutex_unlock(&kv->lock);
}

size_t nyxj_kv_wal_replayed(const nyxj_kv* kv) { return kv ? kv->wal_replayed : 0; }

bool nyxj_kv_checkpoint(nyxj_kv* kv, const char* snapshot_path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!kv || !snapshot_path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    nyxj_mutex_lock(&kv->lock);
    char* text = snapshot_locked(kv);
    bool ok = text != NULL;
    if (!ok) nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL);
    if (ok) ok = write_atomic(snapshot_path, text, strlen(text), err);
    if (ok && kv->wal) { /* The snapshot is durable: start a new, empty log */
        FILE* f = freopen(kv->wal_path, "wb", kv->wal);
        if (f) f = freopen(kv->wal_path, "ab", f);
        kv->wal = f;
        if (!f) {
            nyxj_error_set(err, NYXJ_ERR_IO, "cannot truncate WAL '%s': %s", kv->wal_path, strerror(errno));
            ok = false;
        }
    }
    nyxj_mutex_unlock(&kv->lock);
    free(text);
    return ok;
}
