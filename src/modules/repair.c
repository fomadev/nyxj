#include "../../include/nyxj_repair.h"
#include "../common/buf.h"
#include "../core/scan.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum { S_KEY, S_COLON, S_VALUE, S_AFTER } fstate;

typedef struct {
    bool is_object;
    fstate st;
    bool comma_seen;
} frame;

typedef struct {
    const char* p;
    const char* end;
    nyxj_buf out;
    frame* frames;
    int depth;
    int max_depth;
    bool top_done;
    bool stop;
    nyxj_repair_report* rep;
} rctx;

static void fix(rctx* c, unsigned kind) {
    c->rep->fixes++;
    c->rep->kinds |= kind;
}

static frame* top(rctx* c) { return c->depth > 0 ? &c->frames[c->depth - 1] : NULL; }

static bool at_key_position(rctx* c) {
    frame* f = top(c);
    return f && f->is_object && (f->st == S_KEY || f->st == S_AFTER);
}

/* Emits the separators needed before a value. Returns false when parsing must stop. */
static bool prepare_value(rctx* c) {
    frame* f = top(c);
    if (!f) {
        if (c->top_done) { fix(c, NYXJ_FIX_TRAILING_DATA); c->stop = true; return false; }
        return true;
    }
    if (f->st == S_AFTER) {
        if (!f->comma_seen) fix(c, NYXJ_FIX_MISSING_COMMA);
        nyxj_buf_putc(&c->out, ',');
        f->st = f->is_object ? S_KEY : S_VALUE;
    }
    if (f->is_object) {
        if (f->st == S_KEY) { /* A value where a key was expected */
            fix(c, NYXJ_FIX_GARBAGE);
            nyxj_buf_append(&c->out, "\"\":", 3);
        } else if (f->st == S_COLON) {
            fix(c, NYXJ_FIX_MISSING_COLON);
            nyxj_buf_putc(&c->out, ':');
        }
    }
    f->st = S_VALUE;
    return true;
}

static void value_done(rctx* c) {
    frame* f = top(c);
    if (!f) { c->top_done = true; return; }
    f->st = S_AFTER;
    f->comma_seen = false;
}

static void prepare_key(rctx* c) {
    frame* f = top(c);
    if (f->st == S_AFTER) {
        if (!f->comma_seen) fix(c, NYXJ_FIX_MISSING_COMMA);
        nyxj_buf_putc(&c->out, ',');
    }
    f->st = S_COLON;
}

static void close_pending_member(rctx* c, frame* f) {
    if (!f->is_object) return;
    if (f->st == S_COLON) { fix(c, NYXJ_FIX_MISSING_VALUE); nyxj_buf_append(&c->out, ":null", 5); }
    else if (f->st == S_VALUE) { fix(c, NYXJ_FIX_MISSING_VALUE); nyxj_buf_append(&c->out, "null", 4); }
}

static void close_container(rctx* c, char ch) {
    frame* f = top(c);
    if (!f) { fix(c, NYXJ_FIX_GARBAGE); return; }
    char expected = f->is_object ? '}' : ']';
    if (ch != expected) fix(c, NYXJ_FIX_BRACKET);
    if (f->st == S_AFTER && f->comma_seen) fix(c, NYXJ_FIX_TRAILING_COMMA);
    close_pending_member(c, f);
    nyxj_buf_putc(&c->out, expected);
    c->depth--;
    value_done(c);
}

static void open_container(rctx* c, bool is_object) {
    if (!prepare_value(c)) return;
    if (c->depth >= c->max_depth) { c->stop = true; return; }
    frame* f = &c->frames[c->depth++];
    f->is_object = is_object;
    f->st = is_object ? S_KEY : S_VALUE;
    f->comma_seen = false;
    nyxj_buf_putc(&c->out, is_object ? '{' : '[');
}

static void emit_escaped_control(rctx* c, unsigned char ch) {
    char tmp[8];
    switch (ch) {
        case '\n': nyxj_buf_append(&c->out, "\\n", 2); break;
        case '\r': nyxj_buf_append(&c->out, "\\r", 2); break;
        case '\t': nyxj_buf_append(&c->out, "\\t", 2); break;
        case '\b': nyxj_buf_append(&c->out, "\\b", 2); break;
        case '\f': nyxj_buf_append(&c->out, "\\f", 2); break;
        default: snprintf(tmp, sizeof(tmp), "\\u%04x", ch); nyxj_buf_puts(&c->out, tmp);
    }
}

/* Copies a string literal delimited by `quote`, normalising it to a JSON string. */
static void copy_string(rctx* c) {
    char quote = *c->p++;
    if (quote != '"') fix(c, NYXJ_FIX_QUOTES);
    nyxj_buf_putc(&c->out, '"');
    while (c->p < c->end) {
        unsigned char ch = (unsigned char)*c->p;
        if (ch == (unsigned char)quote) { c->p++; nyxj_buf_putc(&c->out, '"'); return; }
        if (ch == '\\') {
            if (c->end - c->p < 2) { fix(c, NYXJ_FIX_ESCAPE); c->p++; continue; }
            char n = c->p[1];
            if (n && strchr("\"\\/bfnrt", n)) {
                nyxj_buf_append(&c->out, c->p, 2);
                c->p += 2;
            } else if (n == 'u' && c->end - c->p >= 6 && nyxj_hex4(c->p + 2) >= 0) {
                nyxj_buf_append(&c->out, c->p, 6);
                c->p += 6;
            } else if (n == quote) {
                nyxj_buf_putc(&c->out, n);
                c->p += 2;
            } else if (n == '\n') {
                fix(c, NYXJ_FIX_ESCAPE);
                c->p += 2;
            } else {
                fix(c, NYXJ_FIX_ESCAPE);
                nyxj_buf_append(&c->out, "\\\\", 2);
                c->p++;
            }
            continue;
        }
        if (ch == '"') { nyxj_buf_append(&c->out, "\\\"", 2); c->p++; continue; }
        if (ch < 0x20) { fix(c, NYXJ_FIX_ESCAPE); emit_escaped_control(c, ch); c->p++; continue; }
        nyxj_buf_putc(&c->out, (char)ch);
        c->p++;
    }
    fix(c, NYXJ_FIX_UNCLOSED);
    nyxj_buf_putc(&c->out, '"');
}

static void emit_string_n(rctx* c, const char* s, size_t n) {
    char* tmp = malloc(n + 1);
    if (!tmp) { c->out.oom = true; return; }
    memcpy(tmp, s, n);
    tmp[n] = '\0';
    nyxj_buf_json_string(&c->out, tmp, false);
    free(tmp);
}

static bool word_is(const char* s, size_t n, const char* w) { return strlen(w) == n && memcmp(s, w, n) == 0; }

static bool is_word_start(unsigned char ch) { return isalpha(ch) || ch == '_' || ch == '$' || ch >= 0x80; }
static bool is_word_char(unsigned char ch) { return isalnum(ch) || ch == '_' || ch == '$' || ch == '-' || ch == '.' || ch >= 0x80; }

static void handle_word(rctx* c) {
    const char* s = c->p;
    while (c->p < c->end && is_word_char((unsigned char)*c->p)) c->p++;
    size_t n = (size_t)(c->p - s);
    if (at_key_position(c)) {
        prepare_key(c);
        fix(c, NYXJ_FIX_UNQUOTED_KEY);
        emit_string_n(c, s, n);
        return;
    }
    if (!prepare_value(c)) return;
    if (word_is(s, n, "true") || word_is(s, n, "false") || word_is(s, n, "null")) {
        nyxj_buf_append(&c->out, s, n);
    } else if (word_is(s, n, "True") || word_is(s, n, "TRUE")) {
        fix(c, NYXJ_FIX_LITERAL);
        nyxj_buf_puts(&c->out, "true");
    } else if (word_is(s, n, "False") || word_is(s, n, "FALSE")) {
        fix(c, NYXJ_FIX_LITERAL);
        nyxj_buf_puts(&c->out, "false");
    } else if (word_is(s, n, "None") || word_is(s, n, "NULL") || word_is(s, n, "Null") || word_is(s, n, "nil") ||
               word_is(s, n, "undefined") || word_is(s, n, "NaN") || word_is(s, n, "Infinity")) {
        fix(c, NYXJ_FIX_LITERAL);
        nyxj_buf_puts(&c->out, "null");
    } else {
        fix(c, NYXJ_FIX_QUOTES); /* Bare word value: keep it as a string */
        emit_string_n(c, s, n);
    }
    value_done(c);
}

static void handle_number(rctx* c) {
    const char* s = c->p;
    while (c->p < c->end && (isalnum((unsigned char)*c->p) || *c->p == '+' || *c->p == '-' || *c->p == '.')) c->p++;
    size_t n = (size_t)(c->p - s);
    if (at_key_position(c)) {
        prepare_key(c);
        fix(c, NYXJ_FIX_UNQUOTED_KEY);
        emit_string_n(c, s, n);
        return;
    }
    if (!prepare_value(c)) return;
    if (nyxj_scan_number(s, s + n) == s + n) { /* Already valid */
        nyxj_buf_append(&c->out, s, n);
        value_done(c);
        return;
    }
    char norm[96];
    size_t k = 0;
    const char* q = s;
    const char* e = s + n;
    bool neg = false;
    if (q < e && (*q == '+' || *q == '-')) { neg = *q == '-'; q++; }
    if ((size_t)(e - q) == 8 && memcmp(q, "Infinity", 8) == 0) {
        fix(c, NYXJ_FIX_LITERAL);
        nyxj_buf_puts(&c->out, "null");
        value_done(c);
        return;
    }
    if (e - q > 2 && q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) {
        char* hend;
        char hex[32];
        size_t hl = (size_t)(e - q - 2) < sizeof(hex) - 1 ? (size_t)(e - q - 2) : sizeof(hex) - 1;
        memcpy(hex, q + 2, hl);
        hex[hl] = '\0';
        unsigned long long v = strtoull(hex, &hend, 16);
        if (*hend == '\0') {
            fix(c, NYXJ_FIX_NUMBER);
            nyxj_buf_printf(&c->out, "%s%llu", neg ? "-" : "", v);
            value_done(c);
            return;
        }
    }
    if (neg) norm[k++] = '-';
    while (q + 1 < e && *q == '0' && isdigit((unsigned char)q[1])) q++; /* 007 -> 7 */
    if (q < e && *q == '.') norm[k++] = '0';                            /* .5 -> 0.5 */
    for (; q < e && k + 2 < sizeof(norm); q++) {
        if (*q == '.' && (q + 1 >= e || !isdigit((unsigned char)q[1]))) continue; /* 5. -> 5 */
        norm[k++] = *q;
    }
    norm[k] = '\0';
    if (k > 0 && nyxj_scan_number(norm, norm + k) == norm + k) {
        fix(c, NYXJ_FIX_NUMBER);
        nyxj_buf_append(&c->out, norm, k);
    } else {
        fix(c, NYXJ_FIX_QUOTES);
        emit_string_n(c, s, n);
    }
    value_done(c);
}

static void skip_line(rctx* c) {
    while (c->p < c->end && *c->p != '\n') c->p++;
}

char* nyxj_repair(const char* text, size_t len, size_t* out_len, nyxj_repair_report* report) {
    nyxj_repair_report local;
    if (!report) report = &local;
    memset(report, 0, sizeof(*report));
    if (out_len) *out_len = 0;
    if (!text) return NULL;

    rctx c;
    memset(&c, 0, sizeof(c));
    c.p = text;
    c.end = text + len;
    c.max_depth = NYXJ_DEFAULT_MAX_DEPTH;
    c.frames = malloc(sizeof(frame) * (size_t)c.max_depth);
    c.rep = report;
    if (!c.frames) return NULL;
    nyxj_buf_init(&c.out, len + 16);

    if (len >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0) { c.p += 3; fix(&c, NYXJ_FIX_BOM); }

    while (c.p < c.end && !c.stop) {
        unsigned char ch = (unsigned char)*c.p;
        if (nyxj_is_ws((char)ch)) { c.p++; continue; }
        if (ch == '`' && c.end - c.p >= 3 && memcmp(c.p, "```", 3) == 0) { /* Markdown code fence */
            fix(&c, NYXJ_FIX_CODE_FENCE);
            if (c.top_done && c.depth == 0) break;
            skip_line(&c);
            continue;
        }
        if (ch == '/' && c.end - c.p >= 2 && c.p[1] == '/') { fix(&c, NYXJ_FIX_COMMENT); skip_line(&c); continue; }
        if (ch == '#') { fix(&c, NYXJ_FIX_COMMENT); skip_line(&c); continue; }
        if (ch == '/' && c.end - c.p >= 2 && c.p[1] == '*') {
            fix(&c, NYXJ_FIX_COMMENT);
            c.p += 2;
            while (c.end - c.p >= 2 && !(c.p[0] == '*' && c.p[1] == '/')) c.p++;
            c.p = c.end - c.p >= 2 ? c.p + 2 : c.end;
            continue;
        }
        frame* f = top(&c);
        switch (ch) {
            case '{': c.p++; open_container(&c, true); break;
            case '[': c.p++; open_container(&c, false); break;
            case '}': case ']': c.p++; close_container(&c, (char)ch); break;
            case ',':
                c.p++;
                if (!f) { fix(&c, NYXJ_FIX_GARBAGE); break; }
                if (f->is_object && (f->st == S_COLON || f->st == S_VALUE)) {
                    close_pending_member(&c, f);
                    f->st = S_AFTER;
                    f->comma_seen = true;
                } else if (f->st == S_AFTER && !f->comma_seen) {
                    f->comma_seen = true;
                } else {
                    fix(&c, NYXJ_FIX_GARBAGE); /* Leading or doubled comma */
                }
                break;
            case ':': case '=':
                c.p++;
                if (f && f->is_object && f->st == S_COLON) {
                    if (ch == '=') fix(&c, NYXJ_FIX_MISSING_COLON);
                    nyxj_buf_putc(&c.out, ':');
                    f->st = S_VALUE;
                } else {
                    fix(&c, NYXJ_FIX_GARBAGE);
                }
                break;
            case '"': case '\'': case '`':
                if (at_key_position(&c)) {
                    prepare_key(&c);
                    copy_string(&c);
                } else if (prepare_value(&c)) {
                    copy_string(&c);
                    value_done(&c);
                }
                break;
            default:
                if (ch == '-' || ch == '+' || ch == '.' || isdigit(ch)) handle_number(&c);
                else if (is_word_start(ch)) handle_word(&c);
                else { fix(&c, NYXJ_FIX_GARBAGE); c.p++; }
        }
    }

    if (c.depth > 0) fix(&c, NYXJ_FIX_UNCLOSED);
    while (c.depth > 0) {
        frame* f = top(&c);
        if (f->st == S_AFTER && f->comma_seen) fix(&c, NYXJ_FIX_TRAILING_COMMA);
        close_pending_member(&c, f);
        nyxj_buf_putc(&c.out, f->is_object ? '}' : ']');
        c.depth--;
        value_done(&c);
    }
    free(c.frames);
    if (!c.top_done || c.out.oom) {
        nyxj_buf_free(&c.out);
        return NULL;
    }
    return nyxj_buf_detach(&c.out, out_len);
}

nyxj_result nyxj_parse_lenient(const char* text, size_t len, nyxj_repair_report* report) {
    nyxj_result strict = nyxj_parse_ex(text, len, NULL);
    if (strict.is_valid) {
        if (report) memset(report, 0, sizeof(*report));
        return strict;
    }
    size_t n;
    char* fixed = nyxj_repair(text, len, &n, report);
    if (!fixed) return strict;
    nyxj_result r = nyxj_parse_ex(fixed, n, NULL);
    free(fixed);
    return r.is_valid ? r : strict;
}

static const char* kind_names[NYXJ_FIX_KIND_COUNT] = {
    "comment", "quotes", "unquoted_key", "trailing_comma", "missing_comma", "missing_colon",
    "unclosed", "literal", "number", "escape", "garbage", "missing_value",
    "trailing_data", "bracket", "bom", "code_fence"};

const char* nyxj_repair_kind_name(unsigned kind_bit) {
    for (int i = 0; i < NYXJ_FIX_KIND_COUNT; i++)
        if (kind_bit == (1u << i)) return kind_names[i];
    return "unknown";
}

nyxj_value* nyxj_repair_report_to_value(const nyxj_repair_report* report) {
    nyxj_value* o = nyxj_new_object();
    if (!o) return NULL;
    nyxj_object_set_number(o, "fixes", report ? report->fixes : 0);
    nyxj_value* kinds = nyxj_new_array();
    for (int i = 0; report && kinds && i < NYXJ_FIX_KIND_COUNT; i++)
        if (report->kinds & (1u << i)) nyxj_array_push(kinds, nyxj_new_string(kind_names[i]));
    if (kinds) nyxj_object_set(o, "kinds", kinds);
    return o;
}
