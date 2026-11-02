#include "buf.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void nyxj_buf_init(nyxj_buf* b, size_t initial) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->oom = false;
    if (initial) nyxj_buf_reserve(b, initial);
}

bool nyxj_buf_reserve(nyxj_buf* b, size_t extra) {
    if (b->oom) return false;
    if (b->len + extra + 1 <= b->cap) return true;
    size_t ncap = b->cap ? b->cap : 64;
    while (ncap < b->len + extra + 1) ncap *= 2;
    char* p = realloc(b->data, ncap);
    if (!p) { b->oom = true; return false; }
    b->data = p;
    b->cap = ncap;
    return true;
}

void nyxj_buf_putc(nyxj_buf* b, char c) {
    if (!nyxj_buf_reserve(b, 1)) return;
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}

void nyxj_buf_append(nyxj_buf* b, const void* data, size_t n) {
    if (n == 0 || !nyxj_buf_reserve(b, n)) return;
    memcpy(b->data + b->len, data, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void nyxj_buf_puts(nyxj_buf* b, const char* s) { nyxj_buf_append(b, s, strlen(s)); }

void nyxj_buf_printf(nyxj_buf* b, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char small[256];
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(small, sizeof(small), fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return; }
    if ((size_t)n < sizeof(small)) {
        nyxj_buf_append(b, small, (size_t)n);
    } else if (nyxj_buf_reserve(b, (size_t)n)) {
        vsnprintf(b->data + b->len, (size_t)n + 1, fmt, ap2);
        b->len += (size_t)n;
    }
    va_end(ap2);
}

static void put_u16_escape(nyxj_buf* b, unsigned cp) {
    static const char hex[] = "0123456789abcdef";
    char e[6] = {'\\', 'u', hex[(cp >> 12) & 0xF], hex[(cp >> 8) & 0xF], hex[(cp >> 4) & 0xF], hex[cp & 0xF]};
    nyxj_buf_append(b, e, 6);
}

/* Decodes one UTF-8 sequence; returns its length or 0 when invalid. */
static int utf8_decode(const unsigned char* s, unsigned* cp) {
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c >= 0xC2 && c <= 0xDF && (s[1] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if (c >= 0xE0 && c <= 0xEF && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (*cp < 0x800 || (*cp >= 0xD800 && *cp <= 0xDFFF)) return 0;
        return 3;
    }
    if (c >= 0xF0 && c <= 0xF4 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *cp = ((unsigned)(c & 0x07) << 18) | ((unsigned)(s[1] & 0x3F) << 12) |
              ((unsigned)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (*cp < 0x10000 || *cp > 0x10FFFF) return 0;
        return 4;
    }
    return 0;
}

void nyxj_buf_json_string(nyxj_buf* b, const char* str, bool escape_unicode) {
    const unsigned char* s = (const unsigned char*)(str ? str : "");
    nyxj_buf_putc(b, '"');
    const unsigned char* run = s;
    while (*s) {
        unsigned char c = *s;
        bool plain = c >= 0x20 && c != '"' && c != '\\' && (c < 0x80 || !escape_unicode);
        if (plain) { s++; continue; }
        nyxj_buf_append(b, run, (size_t)(s - run));
        switch (c) {
            case '"': nyxj_buf_append(b, "\\\"", 2); s++; break;
            case '\\': nyxj_buf_append(b, "\\\\", 2); s++; break;
            case '\b': nyxj_buf_append(b, "\\b", 2); s++; break;
            case '\f': nyxj_buf_append(b, "\\f", 2); s++; break;
            case '\n': nyxj_buf_append(b, "\\n", 2); s++; break;
            case '\r': nyxj_buf_append(b, "\\r", 2); s++; break;
            case '\t': nyxj_buf_append(b, "\\t", 2); s++; break;
            default:
                if (c < 0x20) {
                    put_u16_escape(b, c);
                    s++;
                } else {
                    unsigned cp;
                    int n = utf8_decode(s, &cp);
                    if (n == 0) { put_u16_escape(b, 0xFFFD); s++; break; }
                    if (cp >= 0x10000) {
                        cp -= 0x10000;
                        put_u16_escape(b, 0xD800 + (cp >> 10));
                        put_u16_escape(b, 0xDC00 + (cp & 0x3FF));
                    } else {
                        put_u16_escape(b, cp);
                    }
                    s += n;
                }
        }
        run = s;
    }
    nyxj_buf_append(b, run, (size_t)(s - run));
    nyxj_buf_putc(b, '"');
}

void nyxj_buf_json_number(nyxj_buf* b, double n) {
    char tmp[40];
    if (n != n || n > 1.7976931348623157e308 || n < -1.7976931348623157e308) {
        nyxj_buf_append(b, "null", 4); /* NaN and Infinity are not representable in JSON */
        return;
    }
    if (n > -9007199254740992.0 && n < 9007199254740992.0 && (double)(long long)n == n) {
        snprintf(tmp, sizeof(tmp), "%lld", (long long)n);
    } else {
        for (int prec = 15; prec <= 17; prec++) {
            snprintf(tmp, sizeof(tmp), "%.*g", prec, n);
            if (strtod(tmp, NULL) == n) break; /* Shortest round-trip representation */
        }
    }
    nyxj_buf_puts(b, tmp);
}

char* nyxj_buf_detach(nyxj_buf* b, size_t* out_len) {
    if (b->oom) { nyxj_buf_free(b); if (out_len) *out_len = 0; return NULL; }
    if (!b->data && !nyxj_buf_reserve(b, 0)) { if (out_len) *out_len = 0; return NULL; }
    b->data[b->len] = '\0';
    char* d = b->data;
    if (out_len) *out_len = b->len;
    b->data = NULL;
    b->len = b->cap = 0;
    return d;
}

void nyxj_buf_free(nyxj_buf* b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}
