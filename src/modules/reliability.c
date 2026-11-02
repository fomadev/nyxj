#include "../../include/nyxj_reliability.h"
#include "../common/thread.h"

#include <string.h>

/* --- Checksum --- */

#define FNV_OFFSET 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL

static uint64_t fnv(uint64_t h, const void* data, size_t n) {
    const unsigned char* p = data;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h;
}

static uint64_t checksum_rec(uint64_t h, const nyxj_value* v) {
    unsigned char tag = v ? (unsigned char)v->type : 0xFF;
    h = fnv(h, &tag, 1);
    if (!v) return h;
    switch (v->type) {
        case NYXJ_BOOL: { unsigned char b = v->as.boolean; h = fnv(h, &b, 1); break; }
        case NYXJ_NUMBER: h = fnv(h, &v->as.number, sizeof(double)); break;
        case NYXJ_STRING: h = fnv(h, v->as.string, strlen(v->as.string) + 1); break;
        case NYXJ_ARRAY:
            h = fnv(h, &v->as.array.count, sizeof(int));
            for (int i = 0; i < v->as.array.count; i++) h = checksum_rec(h, v->as.array.items[i]);
            break;
        case NYXJ_OBJECT:
            h = fnv(h, &v->as.object.count, sizeof(int));
            for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
                h = fnv(h, p->key, strlen(p->key) + 1);
                h = checksum_rec(h, p->value);
            }
            break;
        default: break;
    }
    return h;
}

uint64_t nyxj_checksum(const nyxj_value* v) { return checksum_rec(FNV_OFFSET, v); }

/* --- Shadow execution --- */

typedef struct {
    const char* json;
    size_t len;
    nyxj_parse_options opts;
    nyxj_result result;
} shadow_job;

static void* shadow_run(void* arg) {
    shadow_job* j = arg;
    j->result = nyxj_parse_ex(j->json, j->len, &j->opts);
    return NULL;
}

nyxj_result nyxj_parse_verified(const char* json, size_t len, const nyxj_parse_options* opts) {
    nyxj_parse_options o = opts ? *opts : nyxj_parse_options_default();
    o.arena = NULL; /* Both replicas must be independent heap trees */
    shadow_job shadow = {json, len, o, {0}};
    nyxj_thread t;
    bool threaded = nyxj_thread_create(&t, shadow_run, &shadow);
    nyxj_result primary = nyxj_parse_ex(json, len, &o);
    if (threaded) nyxj_thread_join(t);
    else shadow_run(&shadow);

    bool agree = primary.is_valid == shadow.result.is_valid;
    if (agree && primary.is_valid)
        agree = nyxj_checksum(primary.root) == nyxj_checksum(shadow.result.root) &&
                nyxj_equals(primary.root, shadow.result.root);
    else if (agree)
        agree = primary.error.code == shadow.result.error.code && primary.error.offset == shadow.result.error.offset;
    nyxj_free_value(shadow.result.root);
    if (!agree) {
        nyxj_free_value(primary.root);
        nyxj_result r;
        memset(&r, 0, sizeof(r));
        nyxj_error_set(&r.error, NYXJ_ERR_INTEGRITY, "shadow execution produced diverging results");
        r.error_msg = nyxj_strerror(NYXJ_ERR_INTEGRITY);
        return r;
    }
    return primary;
}

/* --- Triple modular redundancy --- */

nyxj_tmr* nyxj_tmr_create(const nyxj_value* v) {
    if (!v) return NULL;
    nyxj_tmr* t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    for (int i = 0; i < 3; i++) {
        t->replica[i] = nyxj_clone(v);
        if (!t->replica[i]) { nyxj_tmr_free(t); return NULL; }
    }
    return t;
}

const nyxj_value* nyxj_tmr_read(nyxj_tmr* t, bool* repaired) {
    if (repaired) *repaired = false;
    if (!t) return NULL;
    uint64_t h[3];
    for (int i = 0; i < 3; i++) h[i] = nyxj_checksum(t->replica[i]);
    int winner = -1;
    if (h[0] == h[1] && nyxj_equals(t->replica[0], t->replica[1])) winner = 0;
    else if (h[0] == h[2] && nyxj_equals(t->replica[0], t->replica[2])) winner = 0;
    else if (h[1] == h[2] && nyxj_equals(t->replica[1], t->replica[2])) winner = 1;
    if (winner < 0) return NULL; /* No majority: unrecoverable corruption */
    for (int i = 0; i < 3; i++) {
        if (i == winner || (h[i] == h[winner] && nyxj_equals(t->replica[i], t->replica[winner]))) continue;
        nyxj_value* fixed = nyxj_clone(t->replica[winner]);
        if (!fixed) continue;
        nyxj_free_value(t->replica[i]);
        t->replica[i] = fixed;
        t->repairs++;
        if (repaired) *repaired = true;
    }
    return t->replica[winner];
}

bool nyxj_tmr_write(nyxj_tmr* t, const nyxj_value* v) {
    if (!t || !v) return false;
    nyxj_value* fresh[3];
    for (int i = 0; i < 3; i++) {
        fresh[i] = nyxj_clone(v);
        if (!fresh[i]) {
            while (i--) nyxj_free_value(fresh[i]);
            return false;
        }
    }
    for (int i = 0; i < 3; i++) {
        nyxj_free_value(t->replica[i]);
        t->replica[i] = fresh[i];
    }
    return true;
}

void nyxj_tmr_free(nyxj_tmr* t) {
    if (!t) return;
    for (int i = 0; i < 3; i++) nyxj_free_value(t->replica[i]);
    free(t);
}
