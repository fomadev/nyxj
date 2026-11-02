#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj.h"
#include "internal.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

/*
 * Thread-safe recursive descent parser (RFC 8259).
 * All state lives in the context below: no globals, so any number of threads
 * can parse concurrently. Depth, size, string and element limits protect the
 * engine against JSON Denial-of-Service payloads.
 */

typedef struct {
    const char* start;
    const char* p;
    const char* end;
    unsigned flags;
    int depth;
    int max_depth;
    size_t max_string;
    size_t max_elements;
    nyxj_arena* arena;
    nyxj_error* err;
} pctx;

static nyxj_value* parse_value(pctx* c);

/* --- Errors --- */

static void* fail(pctx* c, nyxj_error_code code, const char* at, const char* detail) {
    nyxj_error* e = c->err;
    if (e->code != NYXJ_OK) return NULL; /* Keep the first (root cause) error */
    if (at > c->end) at = c->end;
    e->code = code;
    nyxj_locate(c->start, at, e);
    char what[24] = "";
    if (code == NYXJ_ERR_UNEXPECTED_CHAR && at < c->end) {
        unsigned char ch = (unsigned char)*at;
        if (ch >= 0x20 && ch < 0x7F) snprintf(what, sizeof(what), " '%c'", ch);
        else snprintf(what, sizeof(what), " 0x%02X", ch);
    }
    snprintf(e->message, sizeof(e->message), "%s%s at line %d, column %d%s%s%s", nyxj_strerror(code), what,
             e->line, e->column, detail ? " (" : "", detail ? detail : "", detail ? ")" : "");
    return NULL;
}

/* --- Allocation (heap or arena) --- */

static void* palloc(pctx* c, size_t n) {
    void* p = c->arena ? nyxj_arena_alloc(c->arena, n) : malloc(n);
    if (!p) fail(c, NYXJ_ERR_OUT_OF_MEMORY, c->p, NULL);
    return p;
}

static void pfree(pctx* c, void* p) {
    if (!c->arena) free(p);
}

static nyxj_value* pvalue(pctx* c, nyxj_type type) {
    nyxj_value* v = palloc(c, sizeof(nyxj_value));
    if (!v) return NULL;
    memset(v, 0, sizeof(*v));
    v->type = type;
    if (c->arena) v->flags = NYXJ_VFLAG_ARENA;
    return v;
}

/* --- Lexing --- */

static bool skip_ws(pctx* c) {
    for (;;) {
        c->p = nyxj_scan_ws(c->p, c->end);
        if (!(c->flags & NYXJ_PARSE_ALLOW_COMMENTS) || c->end - c->p < 2 || c->p[0] != '/') return true;
        if (c->p[1] == '/') {
            c->p += 2;
            while (c->p < c->end && *c->p != '\n') c->p++;
        } else if (c->p[1] == '*') {
            const char* open = c->p;
            c->p += 2;
            while (c->end - c->p >= 2 && !(c->p[0] == '*' && c->p[1] == '/')) c->p++;
            if (c->end - c->p < 2) {
                fail(c, NYXJ_ERR_UNEXPECTED_EOF, open, "unterminated comment");
                return false;
            }
            c->p += 2;
        } else {
            return true;
        }
    }
}

static char* parse_string(pctx* c, size_t* out_len) {
    const char* quote = c->p;
    const char* s = quote + 1;
    const char* q = nyxj_scan_string_special(s, c->end);

    if (q < c->end && *q == '"') { /* Fast path: no escapes, single memcpy */
        size_t n = (size_t)(q - s);
        if (c->max_string && n > c->max_string) return fail(c, NYXJ_ERR_STRING_LIMIT, quote, NULL);
        size_t bad = nyxj_utf8_invalid_at((const unsigned char*)s, n);
        if (bad != n) return fail(c, NYXJ_ERR_INVALID_UTF8, s + bad, NULL);
        char* d = palloc(c, n + 1);
        if (!d) return NULL;
        memcpy(d, s, n);
        d[n] = '\0';
        c->p = q + 1;
        *out_len = n;
        return d;
    }

    /* Slow path: locate the closing quote, then decode escapes. */
    const char* r = q;
    for (;;) {
        if (r >= c->end) return fail(c, NYXJ_ERR_INVALID_STRING, quote, "unterminated string");
        if (*r == '"') break;
        if (*r == '\\') {
            if (c->end - r < 2) return fail(c, NYXJ_ERR_INVALID_STRING, quote, "unterminated string");
            r = nyxj_scan_string_special(r + 2, c->end);
            continue;
        }
        return fail(c, NYXJ_ERR_CONTROL_CHAR, r, NULL);
    }
    size_t raw = (size_t)(r - s);
    char* d = palloc(c, raw + 1);
    if (!d) return NULL;
    size_t n = 0;
    const char* x = s;
    while (x < r) {
        const char* y = memchr(x, '\\', (size_t)(r - x));
        if (!y) y = r;
        size_t seg = (size_t)(y - x);
        size_t bad = nyxj_utf8_invalid_at((const unsigned char*)x, seg);
        if (bad != seg) { pfree(c, d); return fail(c, NYXJ_ERR_INVALID_UTF8, x + bad, NULL); }
        memcpy(d + n, x, seg);
        n += seg;
        x = y;
        if (x < r) {
            size_t outn = 0;
            nyxj_error_code code = NYXJ_OK;
            size_t used = nyxj_decode_escape(x, r, d + n, &outn, &code);
            if (!used) {
                pfree(c, d);
                if (code == NYXJ_ERR_UNEXPECTED_EOF) code = x[1] == 'u' ? NYXJ_ERR_INVALID_UNICODE : NYXJ_ERR_INVALID_ESCAPE;
                return fail(c, code, x, NULL);
            }
            n += outn;
            x += used;
        }
    }
    if (c->max_string && n > c->max_string) { pfree(c, d); return fail(c, NYXJ_ERR_STRING_LIMIT, quote, NULL); }
    d[n] = '\0';
    c->p = r + 1;
    *out_len = n;
    return d;
}

static nyxj_value* parse_literal(pctx* c, const char* word, size_t len, nyxj_type type, bool b) {
    if ((size_t)(c->end - c->p) < len || memcmp(c->p, word, len) != 0)
        return fail(c, NYXJ_ERR_INVALID_LITERAL, c->p, NULL);
    nyxj_value* v = pvalue(c, type);
    if (!v) return NULL;
    if (type == NYXJ_BOOL) v->as.boolean = b;
    c->p += len;
    return v;
}

static nyxj_value* parse_number(pctx* c) {
    const char* s = c->p;
    const char* e = nyxj_scan_number(s, c->end);
    if (!e) return fail(c, NYXJ_ERR_INVALID_NUMBER, s, NULL);
    nyxj_value* v = pvalue(c, NYXJ_NUMBER);
    if (!v) return NULL;
    v->as.number = nyxj_number_value(s, (size_t)(e - s));
    c->p = e;
    return v;
}

/* --- Containers --- */

static nyxj_value* parse_array(pctx* c) {
    const char* open = c->p++;
    if (++c->depth > c->max_depth) return fail(c, NYXJ_ERR_DEPTH_LIMIT, open, NULL);
    nyxj_value* arr = pvalue(c, NYXJ_ARRAY);
    if (!arr) return NULL;

    nyxj_value* local[16];
    nyxj_value** items = local;
    int count = 0, cap = 16;

    if (!skip_ws(c)) goto error;
    if (c->p < c->end && *c->p == ']') { c->p++; goto done; }
    for (;;) {
        nyxj_value* item = parse_value(c);
        if (!item) goto error;
        if (count == cap) {
            nyxj_value** grown = items == local ? malloc(sizeof(nyxj_value*) * (size_t)cap * 2)
                                                : realloc(items, sizeof(nyxj_value*) * (size_t)cap * 2);
            if (!grown) { nyxj_free_value(item); fail(c, NYXJ_ERR_OUT_OF_MEMORY, c->p, NULL); goto error; }
            if (items == local) memcpy(grown, local, sizeof(local));
            items = grown;
            cap *= 2;
        }
        items[count++] = item;
        if (c->max_elements && (size_t)count > c->max_elements) { fail(c, NYXJ_ERR_ELEMENT_LIMIT, open, NULL); goto error; }
        if (!skip_ws(c)) goto error;
        if (c->p >= c->end) { fail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, "expected ',' or ']'"); goto error; }
        if (*c->p == ',') {
            c->p++;
            if (c->flags & NYXJ_PARSE_ALLOW_TRAILING_COMMAS) {
                if (!skip_ws(c)) goto error;
                if (c->p < c->end && *c->p == ']') { c->p++; break; }
            }
            continue;
        }
        if (*c->p == ']') { c->p++; break; }
        fail(c, NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected ',' or ']'");
        goto error;
    }

done:
    if (count > 0) {
        int fcap = c->arena ? count : nyxj_array_capacity_for(count);
        nyxj_value** final = palloc(c, sizeof(nyxj_value*) * (size_t)fcap);
        if (!final) goto error;
        memcpy(final, items, sizeof(nyxj_value*) * (size_t)count);
        arr->as.array.items = final;
        arr->as.array.count = count;
        arr->as.array.capacity = fcap;
    }
    if (items != local) free(items);
    c->depth--;
    return arr;

error:
    for (int i = 0; i < count; i++) nyxj_free_value(items[i]);
    if (items != local) free(items);
    pfree(c, arr);
    return NULL;
}

static nyxj_value* parse_object(pctx* c) {
    const char* open = c->p++;
    if (++c->depth > c->max_depth) return fail(c, NYXJ_ERR_DEPTH_LIMIT, open, NULL);
    nyxj_value* obj = pvalue(c, NYXJ_OBJECT);
    if (!obj) return NULL;
    nyxj_pair* last = NULL;

    if (!skip_ws(c)) goto error;
    if (c->p < c->end && *c->p == '}') { c->p++; goto done; }
    for (;;) {
        if (!skip_ws(c)) goto error;
        if (c->p >= c->end) { fail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, "expected a string key"); goto error; }
        if (*c->p != '"') { fail(c, NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected a string key"); goto error; }
        const char* key_at = c->p;
        size_t klen;
        char* key = parse_string(c, &klen);
        if (!key) goto error;
        if (c->flags & NYXJ_PARSE_REJECT_DUPLICATE_KEYS) {
            for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next) {
                if (strcmp(p->key, key) == 0) {
                    pfree(c, key);
                    fail(c, NYXJ_ERR_DUPLICATE_KEY, key_at, NULL);
                    goto error;
                }
            }
        }
        if (!skip_ws(c)) { pfree(c, key); goto error; }
        if (c->p >= c->end || *c->p != ':') {
            pfree(c, key);
            fail(c, c->p >= c->end ? NYXJ_ERR_UNEXPECTED_EOF : NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected ':'");
            goto error;
        }
        c->p++;
        nyxj_value* val = parse_value(c);
        if (!val) { pfree(c, key); goto error; }
        nyxj_pair* pair = palloc(c, sizeof(nyxj_pair));
        if (!pair) { pfree(c, key); nyxj_free_value(val); goto error; }
        pair->key = key;
        pair->value = val;
        pair->next = NULL;
        if (last) last->next = pair;
        else obj->as.object.pairs = pair;
        last = pair;
        obj->as.object.count++;
        if (c->max_elements && (size_t)obj->as.object.count > c->max_elements) {
            fail(c, NYXJ_ERR_ELEMENT_LIMIT, open, NULL);
            goto error;
        }
        if (!skip_ws(c)) goto error;
        if (c->p >= c->end) { fail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, "expected ',' or '}'"); goto error; }
        if (*c->p == ',') {
            c->p++;
            if (c->flags & NYXJ_PARSE_ALLOW_TRAILING_COMMAS) {
                if (!skip_ws(c)) goto error;
                if (c->p < c->end && *c->p == '}') { c->p++; break; }
            }
            continue;
        }
        if (*c->p == '}') { c->p++; break; }
        fail(c, NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected ',' or '}'");
        goto error;
    }

done:
    c->depth--;
    return obj;

error:
    nyxj_free_value(obj);
    return NULL;
}

static nyxj_value* parse_value(pctx* c) {
    if (!skip_ws(c)) return NULL;
    if (c->p >= c->end) return fail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, "expected a value");
    switch (*c->p) {
        case '{': return parse_object(c);
        case '[': return parse_array(c);
        case '"': {
            size_t n;
            char* s = parse_string(c, &n);
            if (!s) return NULL;
            nyxj_value* v = pvalue(c, NYXJ_STRING);
            if (!v) { pfree(c, s); return NULL; }
            v->as.string = s;
            return v;
        }
        case 't': return parse_literal(c, "true", 4, NYXJ_BOOL, true);
        case 'f': return parse_literal(c, "false", 5, NYXJ_BOOL, false);
        case 'n': return parse_literal(c, "null", 4, NYXJ_NULL, false);
        default:
            if (*c->p == '-' || nyxj_is_digit(*c->p)) return parse_number(c);
            return fail(c, NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected a value");
    }
}

/* --- Public API --- */

nyxj_parse_options nyxj_parse_options_default(void) {
    nyxj_parse_options o;
    memset(&o, 0, sizeof(o));
    o.max_depth = NYXJ_DEFAULT_MAX_DEPTH;
    return o;
}

nyxj_result nyxj_parse_ex(const char* json, size_t len, const nyxj_parse_options* opts) {
    nyxj_result r;
    memset(&r, 0, sizeof(r));
    nyxj_parse_options o = opts ? *opts : nyxj_parse_options_default();
    if (!json) {
        nyxj_error_set(&r.error, NYXJ_ERR_NULL_INPUT, NULL);
        r.error_msg = nyxj_strerror(r.error.code);
        return r;
    }
    if (o.max_input_size && len > o.max_input_size) {
        nyxj_error_set(&r.error, NYXJ_ERR_SIZE_LIMIT, "input of %zu bytes exceeds the limit of %zu bytes",
                       len, o.max_input_size);
        r.error_msg = nyxj_strerror(r.error.code);
        return r;
    }
    pctx c;
    c.start = json;
    c.p = json;
    c.end = json + len;
    c.flags = o.flags;
    c.depth = 0;
    c.max_depth = o.max_depth > 0 ? o.max_depth : NYXJ_DEFAULT_MAX_DEPTH;
    c.max_string = o.max_string_length;
    c.max_elements = o.max_elements;
    c.arena = o.arena;
    c.err = &r.error;

    if (len >= 3 && memcmp(json, "\xEF\xBB\xBF", 3) == 0) c.p += 3; /* UTF-8 BOM */

    nyxj_value* root = parse_value(&c);
    if (root) {
        if (!skip_ws(&c) || c.p < c.end) {
            fail(&c, NYXJ_ERR_TRAILING_DATA, c.p, NULL);
            nyxj_free_value(root);
            root = NULL;
        }
    }
    if (!root) {
        r.error_msg = nyxj_strerror(r.error.code);
        return r;
    }
    r.is_valid = true;
    r.root = root;
    return r;
}

nyxj_result nyxj_parse_n(const char* json, size_t len) { return nyxj_parse_ex(json, len, NULL); }

nyxj_result nyxj_parse(const char* json_str) {
    return nyxj_parse_ex(json_str, json_str ? strlen(json_str) : 0, NULL);
}

nyxj_value* nyxj_parse_file(const char* path, const nyxj_parse_options* opts, nyxj_error* err) {
    size_t len;
    char* data = nyxj_read_file(path, &len, err);
    if (!data) return NULL;
    nyxj_result r = nyxj_parse_ex(data, len, opts);
    free(data);
    if (!r.is_valid && err) *err = r.error;
    return r.root;
}
