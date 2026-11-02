#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_sax.h"
#include "../core/scan.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    ST_VALUE,
    ST_ARRAY_FIRST,
    ST_OBJECT_FIRST,
    ST_KEY,
    ST_COLON,
    ST_AFTER_VALUE,
    ST_DONE,
    ST_STRING,
    ST_STRING_ESC,
    ST_STRING_U,
    ST_NUMBER,
    ST_LITERAL,
    ST_ERROR
} sax_state;

struct nyxj_sax {
    nyxj_sax_handler h;
    void* user;
    nyxj_sax_options opt;
    sax_state state;
    /* Container stack: 'O' or 'A' per level, plus per-level element counters */
    char* stack;
    uint32_t* counts;
    int depth;
    /* Current token */
    char* tok;
    size_t tok_len;
    bool str_is_key;
    const char* lit;
    int lit_pos;
    int u_count;
    unsigned u_val;
    unsigned pending_high;
    int utf8_need;
    unsigned char utf8_lo, utf8_hi;
    int bom_pos;
    /* Position tracking */
    uint64_t offset;
    int line, column;
    uint64_t documents;
    nyxj_error err;
};

static bool sfail(nyxj_sax* s, nyxj_error_code code, const char* detail) {
    if (s->state == ST_ERROR) return false;
    s->state = ST_ERROR;
    s->err.code = code;
    s->err.offset = (size_t)s->offset;
    s->err.line = s->line;
    s->err.column = s->column;
    snprintf(s->err.message, sizeof(s->err.message), "%s at line %d, column %d%s%s%s", nyxj_strerror(code), s->line,
             s->column, detail ? " (" : "", detail ? detail : "", detail ? ")" : "");
    return false;
}

#define CB(name, ...) (!s->h.name || s->h.name(s->user, __VA_ARGS__) || sfail(s, NYXJ_ERR_INVALID_ARGUMENT, "aborted by handler"))
#define CB0(name) (!s->h.name || s->h.name(s->user) || sfail(s, NYXJ_ERR_INVALID_ARGUMENT, "aborted by handler"))

nyxj_sax* nyxj_sax_create(const nyxj_sax_handler* handler, void* user, const nyxj_sax_options* opts) {
    nyxj_sax* s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    if (handler) s->h = *handler;
    s->user = user;
    if (opts) s->opt = *opts;
    if (s->opt.max_depth <= 0) s->opt.max_depth = NYXJ_DEFAULT_MAX_DEPTH;
    if (s->opt.max_token_size == 0) s->opt.max_token_size = 64 * 1024;
    s->stack = malloc((size_t)s->opt.max_depth);
    s->counts = malloc(sizeof(uint32_t) * (size_t)s->opt.max_depth);
    s->tok = malloc(s->opt.max_token_size + 1);
    if (!s->stack || !s->counts || !s->tok) { nyxj_sax_destroy(s); return NULL; }
    nyxj_sax_reset(s);
    return s;
}

void nyxj_sax_reset(nyxj_sax* s) {
    if (!s) return;
    s->state = ST_VALUE;
    s->depth = 0;
    s->tok_len = 0;
    s->pending_high = 0;
    s->utf8_need = 0;
    s->bom_pos = 0;
    s->offset = 0;
    s->line = 1;
    s->column = 1;
    s->documents = 0;
    nyxj_error_clear(&s->err);
}

void nyxj_sax_destroy(nyxj_sax* s) {
    if (!s) return;
    free(s->stack);
    free(s->counts);
    free(s->tok);
    free(s);
}

const nyxj_error* nyxj_sax_error(const nyxj_sax* s) { return s ? &s->err : NULL; }
uint64_t nyxj_sax_bytes(const nyxj_sax* s) { return s ? s->offset : 0; }
uint64_t nyxj_sax_documents(const nyxj_sax* s) { return s ? s->documents : 0; }

size_t nyxj_sax_memory(const nyxj_sax* s) {
    if (!s) return 0;
    return sizeof(*s) + (size_t)s->opt.max_depth * (1 + sizeof(uint32_t)) + s->opt.max_token_size + 1;
}

static bool tok_put(nyxj_sax* s, char c) {
    if (s->tok_len >= s->opt.max_token_size) return sfail(s, NYXJ_ERR_STRING_LIMIT, "token larger than max_token_size");
    s->tok[s->tok_len++] = c;
    return true;
}

static bool tok_put_cp(nyxj_sax* s, unsigned cp) {
    char tmp[4];
    size_t n = nyxj_utf8_encode(cp, tmp);
    for (size_t i = 0; i < n; i++)
        if (!tok_put(s, tmp[i])) return false;
    return true;
}

static bool end_value(nyxj_sax* s) {
    if (s->depth == 0) {
        s->documents++;
        s->state = ST_DONE;
        return CB0(on_document_end);
    }
    if (s->opt.max_elements && ++s->counts[s->depth - 1] > s->opt.max_elements)
        return sfail(s, NYXJ_ERR_ELEMENT_LIMIT, NULL);
    s->state = ST_AFTER_VALUE;
    return true;
}

static bool push(nyxj_sax* s, char kind) {
    if (s->depth >= s->opt.max_depth) return sfail(s, NYXJ_ERR_DEPTH_LIMIT, NULL);
    s->stack[s->depth] = kind;
    s->counts[s->depth] = 0;
    s->depth++;
    return true;
}

static bool close_container(nyxj_sax* s, char kind) {
    if (s->depth == 0 || s->stack[s->depth - 1] != kind)
        return sfail(s, NYXJ_ERR_UNEXPECTED_CHAR, "mismatched closing bracket");
    s->depth--;
    if (kind == 'O' ? !CB0(on_object_end) : !CB0(on_array_end)) return false;
    return end_value(s);
}

static bool begin_value(nyxj_sax* s, unsigned char ch) {
    switch (ch) {
        case '{':
            if (!push(s, 'O') || !CB0(on_object_start)) return false;
            s->state = ST_OBJECT_FIRST;
            return true;
        case '[':
            if (!push(s, 'A') || !CB0(on_array_start)) return false;
            s->state = ST_ARRAY_FIRST;
            return true;
        case '"':
            s->str_is_key = false;
            s->tok_len = 0;
            s->state = ST_STRING;
            return true;
        case 't': s->lit = "true"; s->lit_pos = 1; s->state = ST_LITERAL; return true;
        case 'f': s->lit = "false"; s->lit_pos = 1; s->state = ST_LITERAL; return true;
        case 'n': s->lit = "null"; s->lit_pos = 1; s->state = ST_LITERAL; return true;
        default:
            if (ch == '-' || (ch >= '0' && ch <= '9')) {
                s->tok_len = 0;
                s->state = ST_NUMBER;
                return tok_put(s, (char)ch);
            }
            return sfail(s, NYXJ_ERR_UNEXPECTED_CHAR, "expected a value");
    }
}

static bool finish_number(nyxj_sax* s) {
    const char* e = nyxj_scan_number(s->tok, s->tok + s->tok_len);
    if (e != s->tok + s->tok_len) return sfail(s, NYXJ_ERR_INVALID_NUMBER, NULL);
    s->tok[s->tok_len] = '\0';
    double v = nyxj_number_value(s->tok, s->tok_len);
    if (!CB(on_number, v, s->tok, s->tok_len)) return false;
    return end_value(s);
}

static bool finish_string(nyxj_sax* s) {
    s->tok[s->tok_len] = '\0';
    if (s->str_is_key) {
        if (!CB(on_key, s->tok, s->tok_len)) return false;
        s->state = ST_COLON;
        return true;
    }
    if (!CB(on_string, s->tok, s->tok_len)) return false;
    return end_value(s);
}

static bool string_byte(nyxj_sax* s, unsigned char ch) {
    if (s->pending_high) {
        if (ch != '\\') return sfail(s, NYXJ_ERR_INVALID_UNICODE, "high surrogate without low surrogate");
        s->state = ST_STRING_ESC;
        return true;
    }
    if (s->utf8_need > 0) {
        if (ch < s->utf8_lo || ch > s->utf8_hi) return sfail(s, NYXJ_ERR_INVALID_UTF8, NULL);
        s->utf8_need--;
        s->utf8_lo = 0x80;
        s->utf8_hi = 0xBF;
        return tok_put(s, (char)ch);
    }
    if (ch == '"') return finish_string(s);
    if (ch == '\\') { s->state = ST_STRING_ESC; return true; }
    if (ch < 0x20) return sfail(s, NYXJ_ERR_CONTROL_CHAR, NULL);
    if (ch >= 0x80) {
        s->utf8_lo = 0x80;
        s->utf8_hi = 0xBF;
        if (ch >= 0xC2 && ch <= 0xDF) s->utf8_need = 1;
        else if (ch == 0xE0) { s->utf8_need = 2; s->utf8_lo = 0xA0; }
        else if ((ch >= 0xE1 && ch <= 0xEC) || ch == 0xEE || ch == 0xEF) s->utf8_need = 2;
        else if (ch == 0xED) { s->utf8_need = 2; s->utf8_hi = 0x9F; }
        else if (ch == 0xF0) { s->utf8_need = 3; s->utf8_lo = 0x90; }
        else if (ch >= 0xF1 && ch <= 0xF3) s->utf8_need = 3;
        else if (ch == 0xF4) { s->utf8_need = 3; s->utf8_hi = 0x8F; }
        else return sfail(s, NYXJ_ERR_INVALID_UTF8, NULL);
    }
    return tok_put(s, (char)ch);
}

static bool escape_byte(nyxj_sax* s, unsigned char ch) {
    if (s->pending_high && ch != 'u') return sfail(s, NYXJ_ERR_INVALID_UNICODE, "high surrogate without low surrogate");
    char out;
    switch (ch) {
        case '"': out = '"'; break;
        case '\\': out = '\\'; break;
        case '/': out = '/'; break;
        case 'b': out = '\b'; break;
        case 'f': out = '\f'; break;
        case 'n': out = '\n'; break;
        case 'r': out = '\r'; break;
        case 't': out = '\t'; break;
        case 'u':
            s->u_count = 0;
            s->u_val = 0;
            s->state = ST_STRING_U;
            return true;
        default: return sfail(s, NYXJ_ERR_INVALID_ESCAPE, NULL);
    }
    s->state = ST_STRING;
    return tok_put(s, out);
}

static bool unicode_byte(nyxj_sax* s, unsigned char ch) {
    int d;
    if (ch >= '0' && ch <= '9') d = ch - '0';
    else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
    else return sfail(s, NYXJ_ERR_INVALID_UNICODE, NULL);
    s->u_val = (s->u_val << 4) | (unsigned)d;
    if (++s->u_count < 4) return true;
    s->state = ST_STRING;
    unsigned u = s->u_val;
    if (s->pending_high) {
        if (u < 0xDC00 || u > 0xDFFF) return sfail(s, NYXJ_ERR_INVALID_UNICODE, NULL);
        unsigned cp = 0x10000 + ((s->pending_high - 0xD800) << 10) + (u - 0xDC00);
        s->pending_high = 0;
        return tok_put_cp(s, cp);
    }
    if (u >= 0xD800 && u <= 0xDBFF) { s->pending_high = u; return true; }
    if (u >= 0xDC00 && u <= 0xDFFF) return sfail(s, NYXJ_ERR_INVALID_UNICODE, NULL);
    return tok_put_cp(s, u);
}

static bool is_number_char(unsigned char ch) {
    return (ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E';
}

bool nyxj_sax_feed(nyxj_sax* s, const char* data, size_t len) {
    if (!s) return false;
    if (s->state == ST_ERROR) return false;
    size_t i = 0;
    while (i < len) {
        unsigned char ch = (unsigned char)data[i];
        bool ws = ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t';
        bool ok = true;
        bool advance = true;

        if (s->offset < 3 && s->bom_pos == (int)s->offset && s->state == ST_VALUE && s->depth == 0) {
            static const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
            if (ch == bom[s->bom_pos]) { s->bom_pos++; s->offset++; i++; continue; }
            if (s->bom_pos > 0) { sfail(s, NYXJ_ERR_INVALID_UTF8, "truncated byte order mark"); return false; }
        }

        switch (s->state) {
            case ST_VALUE:
                if (!ws) ok = begin_value(s, ch);
                break;
            case ST_ARRAY_FIRST:
                if (ws) break;
                ok = ch == ']' ? close_container(s, 'A') : begin_value(s, ch);
                break;
            case ST_OBJECT_FIRST:
            case ST_KEY:
                if (ws) break;
                if (ch == '"') { s->str_is_key = true; s->tok_len = 0; s->state = ST_STRING; }
                else if (ch == '}' && s->state == ST_OBJECT_FIRST) ok = close_container(s, 'O');
                else ok = sfail(s, NYXJ_ERR_UNEXPECTED_CHAR, "expected a string key");
                break;
            case ST_COLON:
                if (ws) break;
                if (ch == ':') s->state = ST_VALUE;
                else ok = sfail(s, NYXJ_ERR_UNEXPECTED_CHAR, "expected ':'");
                break;
            case ST_AFTER_VALUE:
                if (ws) break;
                if (ch == ',') s->state = s->stack[s->depth - 1] == 'O' ? ST_KEY : ST_VALUE;
                else if (ch == '}') ok = close_container(s, 'O');
                else if (ch == ']') ok = close_container(s, 'A');
                else ok = sfail(s, NYXJ_ERR_UNEXPECTED_CHAR, "expected ',' or a closing bracket");
                break;
            case ST_DONE:
                if (ws) break;
                if (s->opt.multi_document) ok = begin_value(s, ch);
                else ok = sfail(s, NYXJ_ERR_TRAILING_DATA, NULL);
                break;
            case ST_STRING: ok = string_byte(s, ch); break;
            case ST_STRING_ESC: ok = escape_byte(s, ch); break;
            case ST_STRING_U: ok = unicode_byte(s, ch); break;
            case ST_NUMBER:
                if (is_number_char(ch)) ok = tok_put(s, (char)ch);
                else { ok = finish_number(s); advance = false; } /* Re-process the delimiter */
                break;
            case ST_LITERAL:
                if (ch != (unsigned char)s->lit[s->lit_pos]) { ok = sfail(s, NYXJ_ERR_INVALID_LITERAL, NULL); break; }
                if (s->lit[++s->lit_pos] == '\0') {
                    if (s->lit[0] == 'n') ok = CB0(on_null);
                    else ok = CB(on_bool, s->lit[0] == 't');
                    if (ok) ok = end_value(s);
                }
                break;
            case ST_ERROR: return false;
        }
        if (!ok) return false;
        if (advance) {
            i++;
            s->offset++;
            if (ch == '\n') { s->line++; s->column = 1; }
            else s->column++;
        }
    }
    return true;
}

bool nyxj_sax_finish(nyxj_sax* s) {
    if (!s || s->state == ST_ERROR) return false;
    if (s->state == ST_NUMBER && !finish_number(s)) return false;
    if (s->state == ST_DONE) return true;
    if (s->opt.multi_document && s->state == ST_VALUE && s->depth == 0) return true; /* Empty stream */
    return sfail(s, NYXJ_ERR_UNEXPECTED_EOF, NULL);
}

bool nyxj_sax_validate(const char* data, size_t len, const nyxj_sax_options* opts, nyxj_error* err) {
    nyxj_error_clear(err);
    nyxj_sax_options o;
    if (opts) o = *opts;
    else memset(&o, 0, sizeof(o));
    if (o.max_token_size == 0) o.max_token_size = len + 1 < (64u << 20) ? len + 1 : (64u << 20);
    nyxj_sax* s = nyxj_sax_create(NULL, NULL, &o);
    if (!s) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    bool ok = nyxj_sax_feed(s, data, len) && nyxj_sax_finish(s);
    if (!ok && err) *err = s->err;
    nyxj_sax_destroy(s);
    return ok;
}
