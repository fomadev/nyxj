/*
 * libFuzzer / AFL++ harnesses for the NyxJ core (no network code involved).
 *
 * One source, one target per build: compile with -DNYXJ_FUZZ_<TARGET>, where
 * TARGET is PARSE, REPAIR, SAX, QUERY, PATCH, SCHEMA, MSGPACK or UTF8.
 *
 *   make fuzz                      libFuzzer binaries in build/fuzz (needs clang)
 *   make fuzz-run FUZZ_TIME=60     run every target for 60 s on its seed corpus
 *   make fuzz-replay               replay the corpus with any compiler (CI, sanitizers)
 *
 * Inputs that carry two documents (query, patch, schema) are split at the
 * first newline: "<first>\n<second>".
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_msgpack.h"
#include "../../include/nyxj_patch.h"
#include "../../include/nyxj_query.h"
#include "../../include/nyxj_repair.h"
#include "../../include/nyxj_sax.h"
#include "../../include/nyxj_schema.h"
#include "../../include/nyxj_view.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUZZ_MAX_INPUT (256u * 1024u)

/* Aborts with a message: libFuzzer and the replay driver both report it as a crash. */
#define FUZZ_ASSERT(cond)                                                          \
    do {                                                                           \
        if (!(cond)) {                                                             \
            fprintf(stderr, "fuzz invariant failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            abort();                                                               \
        }                                                                          \
    } while (0)

/* NUL-terminated copy of the input (most APIs take C strings too). */
static inline char* dup_input(const uint8_t* data, size_t size) {
    char* s = malloc(size + 1);
    if (!s) return NULL;
    if (size) memcpy(s, data, size);
    s[size] = '\0';
    return s;
}

/* Splits "<a>\n<b>" in place; returns b (or NULL when there is no newline). */
static inline char* split_input(char* s, size_t size, size_t* a_len, size_t* b_len) {
    char* nl = memchr(s, '\n', size);
    if (!nl) return NULL;
    *nl = '\0';
    *a_len = (size_t)(nl - s);
    *b_len = size - *a_len - 1;
    return nl + 1;
}

/* Duplicate keys are rejected so that equality-based invariants stay meaningful. */
static inline nyxj_value* parse_part(const char* s, size_t len) {
    nyxj_parse_options o = nyxj_parse_options_default();
    o.flags = NYXJ_PARSE_REJECT_DUPLICATE_KEYS;
    nyxj_result r = nyxj_parse_ex(s, len, &o);
    return r.is_valid ? r.root : NULL;
}

/* Non-finite numbers break equality-based invariants: 1e400 parses to infinity,
 * which JSON serializes as null, and NaN (possible in MessagePack) is never equal
 * to itself. */
static inline bool all_finite(const nyxj_value* v) {
    if (v->type == NYXJ_NUMBER) return v->as.number - v->as.number == 0;
    if (v->type == NYXJ_ARRAY) {
        NYXJ_ARRAY_FOREACH(v, item) if (!all_finite(item)) return false;
    } else if (v->type == NYXJ_OBJECT) {
        NYXJ_OBJECT_FOREACH(v, pair) if (!all_finite(pair->value)) return false;
    }
    return true;
}

#if defined(NYXJ_FUZZ_PARSE)
static void fuzz_one(const uint8_t* data, size_t size) {
    nyxj_parse_options o = nyxj_parse_options_default();
    unsigned mode = (size && (data[0] & 1)) ? NYXJ_PARSE_RELAXED : 0;
    o.flags = mode | NYXJ_PARSE_REJECT_DUPLICATE_KEYS;
    nyxj_result r = nyxj_parse_ex((const char*)data, size, &o);
    if (r.is_valid) {
        /* Invariant: whatever parses serializes, and the output parses back to an equal tree. */
        nyxj_serialize_options so = {size & 2 ? 2 : 0, (size & 4) != 0, (size & 8) != 0};
        size_t n = 0;
        char* text = nyxj_serialize_ex(r.root, &so, &n);
        FUZZ_ASSERT(text != NULL);
        nyxj_result back = nyxj_parse_n(text, n);
        FUZZ_ASSERT(back.is_valid);
        if (all_finite(r.root)) FUZZ_ASSERT(nyxj_equals(r.root, back.root));
        nyxj_free_value(back.root);
        free(text);
        /* The zero-copy view and the streaming parser agree on validity (strict mode). */
        if (mode == 0) {
            nyxj_view view;
            nyxj_error err;
            memset(&view, 0, sizeof(view));
            FUZZ_ASSERT(nyxj_view_parse(&view, (const char*)data, size, NULL, 0, &err));
            nyxj_view_free(&view);
        }
    } else {
        FUZZ_ASSERT(r.root == NULL && r.error.code != NYXJ_OK);
    }
    nyxj_free_value(r.root);

    /* Arena allocation path. */
    nyxj_arena* arena = nyxj_arena_create(4096);
    if (arena) {
        o.arena = arena;
        o.flags = mode; /* Duplicate keys allowed on this path */
        nyxj_result a = nyxj_parse_ex((const char*)data, size, &o);
        (void)a;
        nyxj_arena_destroy(arena);
    }
}

#elif defined(NYXJ_FUZZ_REPAIR)
static void fuzz_one(const uint8_t* data, size_t size) {
    nyxj_repair_report rep;
    size_t n = 0;
    char* fixed = nyxj_repair((const char*)data, size, &n, &rep);
    free(fixed);
    nyxj_result r = nyxj_parse_lenient((const char*)data, size, &rep);
    nyxj_free_value(r.root);
}

#elif defined(NYXJ_FUZZ_SAX)
static bool on_any(void* u) { (*(uint64_t*)u)++; return true; }
static bool on_str(void* u, const char* s, size_t n) { (void)s; (void)n; (*(uint64_t*)u)++; return true; }
static bool on_num(void* u, double v, const char* raw, size_t n) { (void)v; (void)raw; (void)n; (*(uint64_t*)u)++; return true; }
static bool on_bool(void* u, bool b) { (void)b; (*(uint64_t*)u)++; return true; }

static void fuzz_one(const uint8_t* data, size_t size) {
    nyxj_sax_handler h = {on_any, on_any, on_any, on_any, on_str, on_str, on_num, on_bool, on_any, on_any};
    nyxj_sax_options o;
    memset(&o, 0, sizeof(o));
    o.multi_document = size && (data[0] & 1);
    o.max_token_size = 4096;
    uint64_t events = 0;
    nyxj_sax* sax = nyxj_sax_create(&h, &events, &o);
    if (!sax) return;
    /* Feed in irregular chunks to exercise the cross-chunk token buffers. */
    size_t pos = 0, step = 1;
    bool ok = true;
    while (ok && pos < size) {
        size_t n = size - pos < step ? size - pos : step;
        ok = nyxj_sax_feed(sax, (const char*)data + pos, n);
        pos += n;
        step = step * 3 % 61 + 1;
    }
    if (ok) nyxj_sax_finish(sax);
    nyxj_sax_destroy(sax);

    nyxj_error err;
    bool streamed = nyxj_sax_validate((const char*)data, size, NULL, &err);
    nyxj_result r = nyxj_parse_n((const char*)data, size);
    /* Invariant: one-shot DOM parsing and streaming validation agree. Both
     * apply the default limits, except the 64 KiB SAX token limit. */
    if (size < 65536) FUZZ_ASSERT(streamed == r.is_valid);
    nyxj_free_value(r.root);
}

#elif defined(NYXJ_FUZZ_QUERY)
static void fuzz_one(const uint8_t* data, size_t size) {
    char* s = dup_input(data, size);
    if (!s) return;
    size_t plen, dlen;
    char* doc_text = split_input(s, size, &plen, &dlen);
    if (doc_text) {
        nyxj_value* doc = parse_part(doc_text, dlen);
        nyxj_error err;
        nyxj_value* out = nyxj_query(doc, s, &err);
        nyxj_free_value(out);
        nyxj_value* first = nyxj_get_path(doc, s);
        (void)first;
        nyxj_free_value(doc);
    }
    free(s);
}

#elif defined(NYXJ_FUZZ_PATCH)
static void fuzz_one(const uint8_t* data, size_t size) {
    char* s = dup_input(data, size);
    if (!s) return;
    size_t alen, blen;
    char* b = split_input(s, size, &alen, &blen);
    if (b) {
        nyxj_value* doc = parse_part(s, alen);
        nyxj_value* patch = parse_part(b, blen);
        if (doc && patch) {
            nyxj_value* original = nyxj_clone(doc);
            /* Invariant: diff(a, b) applied to a gives b. */
            nyxj_value* d = nyxj_diff(doc, patch);
            nyxj_value* work = nyxj_clone(doc);
            nyxj_error err;
            if (d && work) {
                FUZZ_ASSERT(nyxj_patch_apply(&work, d, &err));
                FUZZ_ASSERT(nyxj_equals(work, patch));
            }
            nyxj_free_value(work);
            nyxj_free_value(d);
            /* Arbitrary JSON Patch / Merge Patch documents never crash. */
            nyxj_patch_apply(&doc, patch, &err);
            nyxj_value* merged = nyxj_merge_patch(original, patch);
            nyxj_free_value(merged);
            nyxj_value* pointed = nyxj_pointer_get(patch, s);
            (void)pointed;
        }
        nyxj_free_value(doc);
        nyxj_free_value(patch);
    }
    free(s);
}

#elif defined(NYXJ_FUZZ_SCHEMA)
static void fuzz_one(const uint8_t* data, size_t size) {
    char* s = dup_input(data, size);
    if (!s) return;
    size_t alen, blen;
    char* b = split_input(s, size, &alen, &blen);
    if (b) {
        nyxj_value* schema = parse_part(s, alen);
        nyxj_value* inst = parse_part(b, blen);
        if (schema && inst) {
            nyxj_schema_report rep;
            memset(&rep, 0, sizeof(rep));
            nyxj_schema_validate(schema, inst, &rep);
            FUZZ_ASSERT(rep.count >= 0 && rep.count <= NYXJ_SCHEMA_MAX_ISSUES && rep.count <= rep.total);
        }
        nyxj_free_value(schema);
        nyxj_free_value(inst);
    }
    free(s);
}

#elif defined(NYXJ_FUZZ_MSGPACK)
static void fuzz_one(const uint8_t* data, size_t size) {
    nyxj_error err;
    nyxj_value* v = nyxj_msgpack_decode(data, size, &err);
    if (v) {
        /* Invariant: decode -> encode -> decode is stable. */
        size_t n = 0;
        uint8_t* enc = nyxj_msgpack_encode(v, &n);
        FUZZ_ASSERT(enc != NULL);
        nyxj_value* again = nyxj_msgpack_decode(enc, n, &err);
        FUZZ_ASSERT(again != NULL);
        if (all_finite(v)) FUZZ_ASSERT(nyxj_equals(v, again));
        nyxj_free_value(again);
        free(enc);
    }
    nyxj_free_value(v);
    char* json = nyxj_msgpack_to_json(data, size, &err);
    free(json);
}

#elif defined(NYXJ_FUZZ_UTF8)
/* Byte-at-a-time reference decoder (RFC 3629), compared with the SIMD validator. */
static size_t reference_invalid_at(const uint8_t* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint32_t c = s[i], cp, min;
        size_t len;
        if (c < 0x80) { i++; continue; }
        if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
        else return i;
        if (i + len > n) return i;
        for (size_t k = 1; k < len; k++) {
            if ((s[i + k] & 0xC0) != 0x80) return i;
            cp = (cp << 6) | (s[i + k] & 0x3F);
        }
        if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return i;
        i += len;
    }
    return n;
}

static void fuzz_one(const uint8_t* data, size_t size) {
    size_t bad = 0;
    bool ok = nyxj_utf8_validate((const char*)data, size, &bad);
    size_t ref = reference_invalid_at(data, size);
    FUZZ_ASSERT(ok == (ref == size));
    if (!ok) FUZZ_ASSERT(bad == ref);
}

#else
#error "define one NYXJ_FUZZ_<TARGET> macro"
#endif

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > FUZZ_MAX_INPUT) return 0;
    fuzz_one(data, size);
    return 0;
}
