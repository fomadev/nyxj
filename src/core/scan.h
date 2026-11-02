/*
 * Internal scanning primitives shared by the DOM parser, the zero-copy
 * tokenizer and the sanitizer: SIMD-accelerated whitespace and string
 * scanning (SSE2 on x86-64, NEON on AArch64, scalar fallback), UTF-8
 * validation, number grammar and escape decoding.
 */
#ifndef NYXJ_SCAN_H
#define NYXJ_SCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/nyxj.h"

#if !defined(NYXJ_NO_SIMD)
#if defined(__SSE2__) || defined(_M_X64)
#define NYXJ_SIMD_SSE2 1
#include <emmintrin.h>
#elif defined(__aarch64__) && defined(__ARM_NEON)
#define NYXJ_SIMD_NEON 1
#include <arm_neon.h>
#endif
#endif

static inline unsigned nyxj_ctz(unsigned x) {
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_ctz(x);
#else
    unsigned n = 0;
    while (!(x & 1u)) { x >>= 1; n++; }
    return n;
#endif
}

static inline bool nyxj_is_ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }
static inline bool nyxj_is_digit(char c) { return c >= '0' && c <= '9'; }

/* Skips JSON whitespace. */
static inline const char* nyxj_scan_ws(const char* p, const char* end) {
#if defined(NYXJ_SIMD_SSE2)
    while (end - p >= 16) {
        if ((unsigned char)*p > ' ') return p; /* Common case: no whitespace at all */
        __m128i x = _mm_loadu_si128((const __m128i*)(const void*)p);
        __m128i ws = _mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(x, _mm_set1_epi8(' ')), _mm_cmpeq_epi8(x, _mm_set1_epi8('\n'))),
            _mm_or_si128(_mm_cmpeq_epi8(x, _mm_set1_epi8('\r')), _mm_cmpeq_epi8(x, _mm_set1_epi8('\t'))));
        unsigned mask = (unsigned)_mm_movemask_epi8(ws) ^ 0xFFFFu;
        if (mask) return p + nyxj_ctz(mask);
        p += 16;
    }
#elif defined(NYXJ_SIMD_NEON)
    while (end - p >= 16) {
        if ((unsigned char)*p > ' ') return p;
        uint8x16_t x = vld1q_u8((const uint8_t*)p);
        uint8x16_t ws = vorrq_u8(vorrq_u8(vceqq_u8(x, vdupq_n_u8(' ')), vceqq_u8(x, vdupq_n_u8('\n'))),
                                 vorrq_u8(vceqq_u8(x, vdupq_n_u8('\r')), vceqq_u8(x, vdupq_n_u8('\t'))));
        if (vminvq_u8(ws) != 0xFF) break;
        p += 16;
    }
#endif
    while (p < end && nyxj_is_ws(*p)) p++;
    return p;
}

/* Returns the first '"', '\\' or control character (< 0x20) in [p, end), or end. */
static inline const char* nyxj_scan_string_special(const char* p, const char* end) {
#if defined(NYXJ_SIMD_SSE2)
    const __m128i q = _mm_set1_epi8('"');
    const __m128i bs = _mm_set1_epi8('\\');
    const __m128i c1f = _mm_set1_epi8(0x1F);
    while (end - p >= 16) {
        __m128i x = _mm_loadu_si128((const __m128i*)(const void*)p);
        __m128i m = _mm_or_si128(_mm_or_si128(_mm_cmpeq_epi8(x, q), _mm_cmpeq_epi8(x, bs)),
                                 _mm_cmpeq_epi8(_mm_max_epu8(x, c1f), c1f));
        unsigned mask = (unsigned)_mm_movemask_epi8(m);
        if (mask) return p + nyxj_ctz(mask);
        p += 16;
    }
#elif defined(NYXJ_SIMD_NEON)
    const uint8x16_t q = vdupq_n_u8('"');
    const uint8x16_t bs = vdupq_n_u8('\\');
    const uint8x16_t c1f = vdupq_n_u8(0x1F);
    while (end - p >= 16) {
        uint8x16_t x = vld1q_u8((const uint8_t*)p);
        uint8x16_t m = vorrq_u8(vorrq_u8(vceqq_u8(x, q), vceqq_u8(x, bs)), vcleq_u8(x, c1f));
        if (vmaxvq_u8(m)) break;
        p += 16;
    }
#endif
    while (p < end) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\' || c < 0x20) return p;
        p++;
    }
    return end;
}

/* Returns the index of the first invalid UTF-8 byte, or n when the span is valid. */
static inline size_t nyxj_utf8_invalid_at(const unsigned char* s, size_t n) {
    size_t i = 0;
    while (i < n) {
        if (i + 8 <= n) { /* ASCII fast path, 8 bytes at a time */
            uint64_t w;
            memcpy(&w, s + i, 8);
            if ((w & 0x8080808080808080ULL) == 0) { i += 8; continue; }
        }
        unsigned char c = s[i];
        if (c < 0x80) { i++; continue; }
        size_t need;
        unsigned char lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c == 0xE0) { need = 2; lo = 0xA0; }
        else if (c >= 0xE1 && c <= 0xEC) need = 2;
        else if (c == 0xED) { need = 2; hi = 0x9F; } /* No UTF-16 surrogates */
        else if (c >= 0xEE && c <= 0xEF) need = 2;
        else if (c == 0xF0) { need = 3; lo = 0x90; }
        else if (c >= 0xF1 && c <= 0xF3) need = 3;
        else if (c == 0xF4) { need = 3; hi = 0x8F; }
        else return i;
        if (i + need >= n) return i; /* Truncated sequence */
        if (s[i + 1] < lo || s[i + 1] > hi) return i;
        for (size_t k = 2; k <= need; k++)
            if ((s[i + k] & 0xC0) != 0x80) return i;
        i += need + 1;
    }
    return n;
}

/* Validates the JSON number grammar; returns the end of the number or NULL. */
static inline const char* nyxj_scan_number(const char* p, const char* end) {
    const char* q = p;
    if (q < end && *q == '-') q++;
    if (q >= end) return NULL;
    if (*q == '0') {
        q++;
    } else if (*q >= '1' && *q <= '9') {
        while (q < end && nyxj_is_digit(*q)) q++;
    } else {
        return NULL;
    }
    if (q < end && *q == '.') {
        q++;
        if (q >= end || !nyxj_is_digit(*q)) return NULL;
        while (q < end && nyxj_is_digit(*q)) q++;
    }
    if (q < end && (*q == 'e' || *q == 'E')) {
        q++;
        if (q < end && (*q == '+' || *q == '-')) q++;
        if (q >= end || !nyxj_is_digit(*q)) return NULL;
        while (q < end && nyxj_is_digit(*q)) q++;
    }
    return q;
}

/* Converts a validated number token to double without reading past len. */
static inline double nyxj_number_value(const char* p, size_t len) {
    /* Fast path: small integers are exact and need no strtod. */
    size_t i = 0;
    bool neg = false;
    if (len && p[0] == '-') { neg = true; i = 1; }
    if (len - i <= 15) {
        uint64_t acc = 0;
        size_t k = i;
        for (; k < len && nyxj_is_digit(p[k]); k++) acc = acc * 10 + (uint64_t)(p[k] - '0');
        if (k == len) return neg ? -(double)acc : (double)acc;
    }
    char small[64];
    char* tmp = len < sizeof(small) ? small : malloc(len + 1);
    if (!tmp) return 0.0;
    memcpy(tmp, p, len);
    tmp[len] = '\0';
    double v = strtod(tmp, NULL);
    if (tmp != small) free(tmp);
    return v;
}

static inline int nyxj_hex4(const char* p) {
    int v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return -1;
    }
    return v;
}

static inline size_t nyxj_utf8_encode(unsigned cp, char* out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/*
 * Decodes the escape sequence starting at p (p[0] == '\\') into out.
 * Returns the number of input bytes consumed, or 0 on error (*code set).
 * When out is NULL the escape is only validated.
 */
static inline size_t nyxj_decode_escape(const char* p, const char* end, char* out, size_t* out_n,
                                        nyxj_error_code* code) {
    if (end - p < 2) { *code = NYXJ_ERR_UNEXPECTED_EOF; return 0; }
    char c = p[1];
    char single = 0;
    switch (c) {
        case '"': single = '"'; break;
        case '\\': single = '\\'; break;
        case '/': single = '/'; break;
        case 'b': single = '\b'; break;
        case 'f': single = '\f'; break;
        case 'n': single = '\n'; break;
        case 'r': single = '\r'; break;
        case 't': single = '\t'; break;
        case 'u': break;
        default: *code = NYXJ_ERR_INVALID_ESCAPE; return 0;
    }
    if (c != 'u') {
        if (out) out[0] = single;
        *out_n = 1;
        return 2;
    }
    if (end - p < 6) { *code = NYXJ_ERR_UNEXPECTED_EOF; return 0; }
    int cp = nyxj_hex4(p + 2);
    if (cp < 0) { *code = NYXJ_ERR_INVALID_UNICODE; return 0; }
    size_t used = 6;
    if (cp >= 0xD800 && cp <= 0xDBFF) {
        if (end - p < 12 || p[6] != '\\' || p[7] != 'u') { *code = NYXJ_ERR_INVALID_UNICODE; return 0; }
        int lo = nyxj_hex4(p + 8);
        if (lo < 0xDC00 || lo > 0xDFFF) { *code = NYXJ_ERR_INVALID_UNICODE; return 0; }
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        used = 12;
    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
        *code = NYXJ_ERR_INVALID_UNICODE; /* Lone low surrogate */
        return 0;
    }
    char tmp[4];
    *out_n = nyxj_utf8_encode((unsigned)cp, out ? out : tmp);
    return used;
}

/* Fills line/column for an error located at pos. */
static inline void nyxj_locate(const char* start, const char* pos, nyxj_error* err) {
    int line = 1, col = 1;
    for (const char* p = start; p < pos; p++) {
        if (*p == '\n') { line++; col = 1; }
        else col++;
    }
    err->offset = (size_t)(pos - start);
    err->line = line;
    err->column = col;
}

#endif
