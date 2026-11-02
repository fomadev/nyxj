#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_view.h"
#include "../core/scan.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char* start;
    const char* p;
    const char* end;
    nyxj_view* v;
    int depth;
    nyxj_error* err;
} tctx;

static bool tfail(tctx* c, nyxj_error_code code, const char* at, const char* detail) {
    if (c->err && c->err->code == NYXJ_OK) {
        c->err->code = code;
        nyxj_locate(c->start, at, c->err);
        snprintf(c->err->message, sizeof(c->err->message), "%s at line %d, column %d%s%s%s", nyxj_strerror(code),
                 c->err->line, c->err->column, detail ? " (" : "", detail ? detail : "", detail ? ")" : "");
    }
    return false;
}

static long emit(tctx* c, nyxj_tok_type type, const char* s, const char* e) {
    nyxj_view* v = c->v;
    if (v->count == v->capacity) {
        if (!v->owns_tokens) { tfail(c, NYXJ_ERR_ELEMENT_LIMIT, s, "token buffer is full"); return -1; }
        size_t ncap = v->capacity ? v->capacity * 2 : 64;
        nyxj_tok* g = realloc(v->tokens, sizeof(nyxj_tok) * ncap);
        if (!g) { tfail(c, NYXJ_ERR_OUT_OF_MEMORY, s, NULL); return -1; }
        v->tokens = g;
        v->capacity = ncap;
    }
    nyxj_tok* t = &v->tokens[v->count];
    t->type = type;
    t->start = (uint32_t)(s - c->start);
    t->end = (uint32_t)(e - c->start);
    t->size = 0;
    t->next = (uint32_t)(v->count + 1);
    t->escaped = false;
    return (long)v->count++;
}

static bool tok_string(tctx* c) {
    const char* quote = c->p;
    const char* s = quote + 1;
    const char* r = s;
    bool escaped = false;
    for (;;) {
        r = nyxj_scan_string_special(r, c->end);
        if (r >= c->end) return tfail(c, NYXJ_ERR_INVALID_STRING, quote, "unterminated string");
        if (*r == '"') break;
        if (*r == '\\') {
            size_t outn;
            nyxj_error_code code = NYXJ_OK;
            size_t used = nyxj_decode_escape(r, c->end, NULL, &outn, &code);
            if (!used) return tfail(c, code, r, NULL);
            escaped = true;
            r += used;
            continue;
        }
        return tfail(c, NYXJ_ERR_CONTROL_CHAR, r, NULL);
    }
    size_t bad = nyxj_utf8_invalid_at((const unsigned char*)s, (size_t)(r - s));
    if (bad != (size_t)(r - s)) return tfail(c, NYXJ_ERR_INVALID_UTF8, s + bad, NULL);
    long idx = emit(c, NYXJ_TOK_STRING, s, r);
    if (idx < 0) return false;
    c->v->tokens[idx].escaped = escaped;
    c->p = r + 1;
    return true;
}

static bool tok_value(tctx* c);

static bool tok_container(tctx* c, bool is_object) {
    const char* open = c->p;
    if (++c->depth > c->v->max_depth) return tfail(c, NYXJ_ERR_DEPTH_LIMIT, open, NULL);
    long idx = emit(c, is_object ? NYXJ_TOK_OBJECT : NYXJ_TOK_ARRAY, open, open);
    if (idx < 0) return false;
    char close = is_object ? '}' : ']';
    c->p++;
    c->p = nyxj_scan_ws(c->p, c->end);
    uint32_t size = 0;
    if (c->p < c->end && *c->p == close) {
        c->p++;
    } else {
        for (;;) {
            c->p = nyxj_scan_ws(c->p, c->end);
            if (is_object) {
                if (c->p >= c->end || *c->p != '"')
                    return tfail(c, c->p >= c->end ? NYXJ_ERR_UNEXPECTED_EOF : NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected a string key");
                if (!tok_string(c)) return false;
                c->p = nyxj_scan_ws(c->p, c->end);
                if (c->p >= c->end || *c->p != ':')
                    return tfail(c, c->p >= c->end ? NYXJ_ERR_UNEXPECTED_EOF : NYXJ_ERR_UNEXPECTED_CHAR, c->p, "expected ':'");
                c->p++;
            }
            if (!tok_value(c)) return false;
            size++;
            c->p = nyxj_scan_ws(c->p, c->end);
            if (c->p >= c->end) return tfail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, NULL);
            if (*c->p == ',') { c->p++; continue; }
            if (*c->p == close) { c->p++; break; }
            return tfail(c, NYXJ_ERR_UNEXPECTED_CHAR, c->p, is_object ? "expected ',' or '}'" : "expected ',' or ']'");
        }
    }
    nyxj_tok* t = &c->v->tokens[idx];
    t->end = (uint32_t)(c->p - c->start);
    t->size = size;
    t->next = (uint32_t)c->v->count;
    c->depth--;
    return true;
}

static bool tok_literal(tctx* c, const char* word, size_t n, nyxj_tok_type type) {
    if ((size_t)(c->end - c->p) < n || memcmp(c->p, word, n) != 0) return tfail(c, NYXJ_ERR_INVALID_LITERAL, c->p, NULL);
    if (emit(c, type, c->p, c->p + n) < 0) return false;
    c->p += n;
    return true;
}

static bool tok_value(tctx* c) {
    c->p = nyxj_scan_ws(c->p, c->end);
    if (c->p >= c->end) return tfail(c, NYXJ_ERR_UNEXPECTED_EOF, c->p, "expected a value");
    switch (*c->p) {
        case '{': return tok_container(c, true);
        case '[': return tok_container(c, false);
        case '"': return tok_string(c);
        case 't': return tok_literal(c, "true", 4, NYXJ_TOK_TRUE);
        case 'f': return tok_literal(c, "false", 5, NYXJ_TOK_FALSE);
        case 'n': return tok_literal(c, "null", 4, NYXJ_TOK_NULL);
        default: {
            const char* e = nyxj_scan_number(c->p, c->end);
            if (!e) return tfail(c, *c->p == '-' || nyxj_is_digit(*c->p) ? NYXJ_ERR_INVALID_NUMBER : NYXJ_ERR_UNEXPECTED_CHAR, c->p, NULL);
            if (emit(c, NYXJ_TOK_NUMBER, c->p, e) < 0) return false;
            c->p = e;
            return true;
        }
    }
}

bool nyxj_view_parse(nyxj_view* view, const char* json, size_t len, nyxj_tok* tokens, size_t capacity, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!view || !json) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    int max_depth = view->max_depth > 0 ? view->max_depth : NYXJ_DEFAULT_MAX_DEPTH;
    memset(view, 0, sizeof(*view));
    view->max_depth = max_depth;
    if (len > 0xFFFFFFF0u) { nyxj_error_set(err, NYXJ_ERR_SIZE_LIMIT, "views are limited to 4 GiB inputs"); return false; }
    view->json = json;
    view->len = len;
    view->tokens = tokens;
    view->capacity = tokens ? capacity : 0;
    view->owns_tokens = tokens == NULL;
    tctx c = {json, json, json + len, view, 0, err};
    if (len >= 3 && memcmp(json, "\xEF\xBB\xBF", 3) == 0) c.p += 3;
    bool ok = tok_value(&c);
    if (ok) {
        c.p = nyxj_scan_ws(c.p, c.end);
        if (c.p < c.end) ok = tfail(&c, NYXJ_ERR_TRAILING_DATA, c.p, NULL);
    }
    if (!ok) {
        if (view->owns_tokens) free(view->tokens);
        view->tokens = NULL;
        view->count = 0;
    }
    return ok;
}

void nyxj_view_free(nyxj_view* view) {
    if (!view) return;
    if (view->owns_tokens) free(view->tokens);
    view->tokens = NULL;
    view->count = view->capacity = 0;
}

/* --- Navigation --- */

static bool valid_tok(const nyxj_view* v, size_t t) { return v && v->tokens && t < v->count; }

const char* nyxj_view_raw(const nyxj_view* v, size_t tok, size_t* len) {
    if (!valid_tok(v, tok)) { if (len) *len = 0; return NULL; }
    if (len) *len = v->tokens[tok].end - v->tokens[tok].start;
    return v->json + v->tokens[tok].start;
}

size_t nyxj_view_string(const nyxj_view* v, size_t tok, char* out, size_t out_size) {
    if (!valid_tok(v, tok) || v->tokens[tok].type != NYXJ_TOK_STRING) return 0;
    const char* s = v->json + v->tokens[tok].start;
    const char* e = v->json + v->tokens[tok].end;
    size_t n = 0;
    char tmp[4];
    while (s < e) {
        size_t produced = 1;
        const char* chunk = s;
        if (*s == '\\') {
            nyxj_error_code code;
            size_t used = nyxj_decode_escape(s, e, tmp, &produced, &code);
            if (!used) break;
            chunk = tmp;
            s += used;
        } else {
            s++;
        }
        for (size_t k = 0; k < produced; k++, n++)
            if (out && n + 1 < out_size) out[n] = chunk[k];
    }
    if (out && out_size) out[n < out_size ? n : out_size - 1] = '\0';
    return n;
}

bool nyxj_view_eq(const nyxj_view* v, size_t tok, const char* s) {
    if (!valid_tok(v, tok) || !s || v->tokens[tok].type != NYXJ_TOK_STRING) return false;
    const nyxj_tok* t = &v->tokens[tok];
    size_t n = strlen(s);
    if (!t->escaped) return t->end - t->start == n && memcmp(v->json + t->start, s, n) == 0;
    char small[256];
    char* buf = n + 2 <= sizeof(small) ? small : malloc(n + 2);
    if (!buf) return false;
    size_t got = nyxj_view_string(v, tok, buf, n + 2);
    bool eq = got == n && memcmp(buf, s, n) == 0;
    if (buf != small) free(buf);
    return eq;
}

size_t nyxj_view_find(const nyxj_view* v, size_t obj, const char* key) {
    if (!valid_tok(v, obj) || v->tokens[obj].type != NYXJ_TOK_OBJECT) return NYXJ_VIEW_NONE;
    size_t child = obj + 1;
    for (uint32_t i = 0; i < v->tokens[obj].size; i++) {
        size_t val = child + 1;
        if (nyxj_view_eq(v, child, key)) return val;
        child = v->tokens[val].next;
    }
    return NYXJ_VIEW_NONE;
}

size_t nyxj_view_index(const nyxj_view* v, size_t arr, size_t i) {
    if (!valid_tok(v, arr) || v->tokens[arr].type != NYXJ_TOK_ARRAY || i >= v->tokens[arr].size) return NYXJ_VIEW_NONE;
    size_t child = arr + 1;
    while (i--) child = v->tokens[child].next;
    return child;
}

size_t nyxj_view_path(const nyxj_view* v, const char* path) {
    if (!v || !v->count || !path) return NYXJ_VIEW_NONE;
    size_t cur = 0;
    const char* p = path;
    if (*p == '$') p++;
    if (*p == '.') p++;
    char key[256];
    while (*p && cur != NYXJ_VIEW_NONE) {
        if (*p == '[') {
            size_t idx = 0;
            p++;
            if (*p < '0' || *p > '9') return NYXJ_VIEW_NONE;
            while (*p >= '0' && *p <= '9') idx = idx * 10 + (size_t)(*p++ - '0');
            if (*p++ != ']') return NYXJ_VIEW_NONE;
            cur = nyxj_view_index(v, cur, idx);
        } else {
            size_t n = 0;
            while (*p && *p != '.' && *p != '[') {
                if (n + 1 >= sizeof(key)) return NYXJ_VIEW_NONE;
                key[n++] = *p++;
            }
            key[n] = '\0';
            cur = nyxj_view_find(v, cur, key);
        }
        if (*p == '.') p++;
    }
    return cur;
}

double nyxj_view_number(const nyxj_view* v, size_t tok) {
    if (!valid_tok(v, tok) || v->tokens[tok].type != NYXJ_TOK_NUMBER) return 0.0;
    return nyxj_number_value(v->json + v->tokens[tok].start, v->tokens[tok].end - v->tokens[tok].start);
}

bool nyxj_view_bool(const nyxj_view* v, size_t tok) {
    return valid_tok(v, tok) && v->tokens[tok].type == NYXJ_TOK_TRUE;
}

nyxj_value* nyxj_view_to_value(const nyxj_view* v, size_t tok) {
    if (!valid_tok(v, tok)) return NULL;
    const nyxj_tok* t = &v->tokens[tok];
    switch (t->type) {
        case NYXJ_TOK_NULL: return nyxj_new_null();
        case NYXJ_TOK_TRUE: return nyxj_new_bool(true);
        case NYXJ_TOK_FALSE: return nyxj_new_bool(false);
        case NYXJ_TOK_NUMBER: return nyxj_new_number(nyxj_view_number(v, tok));
        case NYXJ_TOK_STRING: {
            if (!t->escaped) return nyxj_new_stringn(v->json + t->start, t->end - t->start);
            size_t n = nyxj_view_string(v, tok, NULL, 0);
            char* buf = malloc(n + 1);
            if (!buf) return NULL;
            nyxj_view_string(v, tok, buf, n + 1);
            nyxj_value* s = nyxj_new_stringn(buf, n);
            free(buf);
            return s;
        }
        case NYXJ_TOK_ARRAY: {
            nyxj_value* arr = nyxj_new_array();
            size_t child = tok + 1;
            for (uint32_t i = 0; arr && i < t->size; i++) {
                nyxj_value* item = nyxj_view_to_value(v, child);
                if (!item || !nyxj_array_push(arr, item)) { nyxj_free_value(item); nyxj_free_value(arr); return NULL; }
                child = v->tokens[child].next;
            }
            return arr;
        }
        case NYXJ_TOK_OBJECT: {
            nyxj_value* obj = nyxj_new_object();
            size_t child = tok + 1;
            for (uint32_t i = 0; obj && i < t->size; i++) {
                size_t kn = nyxj_view_string(v, child, NULL, 0);
                char* key = malloc(kn + 1);
                nyxj_value* val = nyxj_view_to_value(v, child + 1);
                if (key) nyxj_view_string(v, child, key, kn + 1);
                if (!key || !val || !nyxj_object_set(obj, key, val)) {
                    free(key);
                    nyxj_free_value(val);
                    nyxj_free_value(obj);
                    return NULL;
                }
                free(key);
                child = v->tokens[child + 1].next;
            }
            return obj;
        }
        default: return NULL;
    }
}
