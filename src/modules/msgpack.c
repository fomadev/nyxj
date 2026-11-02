#include "../../include/nyxj_msgpack.h"
#include "../common/buf.h"
#include "../core/internal.h"

#include <string.h>

#define MSGPACK_MAX_DEPTH NYXJ_DEFAULT_MAX_DEPTH

/* --- Encoder --- */

static void put_be(nyxj_buf* b, uint64_t v, int bytes) {
    char tmp[8];
    for (int i = 0; i < bytes; i++) tmp[i] = (char)(v >> (8 * (bytes - 1 - i)));
    nyxj_buf_append(b, tmp, (size_t)bytes);
}

static void put_tag(nyxj_buf* b, uint8_t tag) { nyxj_buf_putc(b, (char)tag); }

static void encode_len(nyxj_buf* b, size_t n, uint8_t fix_base, size_t fix_max, uint8_t t8, uint8_t t16, uint8_t t32) {
    if (n <= fix_max && fix_base) put_tag(b, (uint8_t)(fix_base | n));
    else if (t8 && n <= 0xFF) { put_tag(b, t8); put_be(b, n, 1); }
    else if (n <= 0xFFFF) { put_tag(b, t16); put_be(b, n, 2); }
    else { put_tag(b, t32); put_be(b, n, 4); }
}

static void encode_number(nyxj_buf* b, double d) {
    if (d >= 0 && d < 18446744073709551616.0 && d == (double)(uint64_t)d) {
        uint64_t u = (uint64_t)d;
        if (u <= 0x7F) put_tag(b, (uint8_t)u);
        else if (u <= 0xFF) { put_tag(b, 0xCC); put_be(b, u, 1); }
        else if (u <= 0xFFFF) { put_tag(b, 0xCD); put_be(b, u, 2); }
        else if (u <= 0xFFFFFFFFULL) { put_tag(b, 0xCE); put_be(b, u, 4); }
        else { put_tag(b, 0xCF); put_be(b, u, 8); }
        return;
    }
    if (d < 0 && d >= -9223372036854775808.0 && d == (double)(int64_t)d) {
        int64_t i = (int64_t)d;
        if (i >= -32) put_tag(b, (uint8_t)(int8_t)i);
        else if (i >= -128) { put_tag(b, 0xD0); put_be(b, (uint64_t)i, 1); }
        else if (i >= -32768) { put_tag(b, 0xD1); put_be(b, (uint64_t)i, 2); }
        else if (i >= -2147483648LL) { put_tag(b, 0xD2); put_be(b, (uint64_t)i, 4); }
        else { put_tag(b, 0xD3); put_be(b, (uint64_t)i, 8); }
        return;
    }
    float f = (float)d;
    if ((double)f == d) { /* Exact in single precision: use 5 bytes instead of 9 */
        uint32_t bits;
        memcpy(&bits, &f, 4);
        put_tag(b, 0xCA);
        put_be(b, bits, 4);
    } else {
        uint64_t bits;
        memcpy(&bits, &d, 8);
        put_tag(b, 0xCB);
        put_be(b, bits, 8);
    }
}

static void encode_str(nyxj_buf* b, const char* s) {
    size_t n = strlen(s);
    encode_len(b, n, 0xA0, 31, 0xD9, 0xDA, 0xDB);
    nyxj_buf_append(b, s, n);
}

static void encode_value(nyxj_buf* b, const nyxj_value* v) {
    if (!v) { put_tag(b, 0xC0); return; }
    switch (v->type) {
        case NYXJ_NULL: put_tag(b, 0xC0); break;
        case NYXJ_BOOL: put_tag(b, v->as.boolean ? 0xC3 : 0xC2); break;
        case NYXJ_NUMBER: encode_number(b, v->as.number); break;
        case NYXJ_STRING: encode_str(b, v->as.string); break;
        case NYXJ_ARRAY:
            encode_len(b, (size_t)v->as.array.count, 0x90, 15, 0, 0xDC, 0xDD);
            for (int i = 0; i < v->as.array.count; i++) encode_value(b, v->as.array.items[i]);
            break;
        case NYXJ_OBJECT:
            encode_len(b, (size_t)v->as.object.count, 0x80, 15, 0, 0xDE, 0xDF);
            for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
                encode_str(b, p->key);
                encode_value(b, p->value);
            }
            break;
    }
}

uint8_t* nyxj_msgpack_encode(const nyxj_value* v, size_t* out_len) {
    nyxj_buf b;
    nyxj_buf_init(&b, 256);
    encode_value(&b, v);
    return (uint8_t*)nyxj_buf_detach(&b, out_len);
}

/* --- Decoder --- */

typedef struct {
    const uint8_t* p;
    const uint8_t* end;
    const uint8_t* start;
    int depth;
    nyxj_error* err;
} dctx;

static nyxj_value* dfail(dctx* c, const char* what) {
    if (c->err && c->err->code == NYXJ_OK) {
        nyxj_error_set(c->err, NYXJ_ERR_MSGPACK, "%s at byte %zu", what, (size_t)(c->p - c->start));
        c->err->offset = (size_t)(c->p - c->start);
    }
    return NULL;
}

static bool need(dctx* c, size_t n) { return (size_t)(c->end - c->p) >= n; }

static uint64_t get_be(dctx* c, int bytes) {
    uint64_t v = 0;
    for (int i = 0; i < bytes; i++) v = (v << 8) | c->p[i];
    c->p += bytes;
    return v;
}

static nyxj_value* decode_value(dctx* c);

static char* decode_raw(dctx* c, size_t n) {
    if (!need(c, n)) { dfail(c, "truncated string"); return NULL; }
    char* s = nyxj_strndup((const char*)c->p, n);
    if (!s) { nyxj_error_set(c->err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    c->p += n;
    return s;
}

static bool read_len(dctx* c, int bytes, size_t* n) {
    if (!need(c, (size_t)bytes)) return false;
    *n = (size_t)get_be(c, bytes);
    return true;
}

static nyxj_value* decode_string_n(dctx* c, size_t n) {
    char* s = decode_raw(c, n);
    if (!s) return NULL;
    nyxj_value* v = nyxj_new_null();
    if (!v) { free(s); return NULL; }
    v->type = NYXJ_STRING;
    v->as.string = s;
    return v;
}

static nyxj_value* decode_array_n(dctx* c, size_t n) {
    if (n > (size_t)(c->end - c->p)) return dfail(c, "array length exceeds the input");
    if (++c->depth > MSGPACK_MAX_DEPTH) return dfail(c, "maximum nesting depth exceeded");
    nyxj_value* arr = nyxj_new_array();
    if (!arr) return NULL;
    for (size_t i = 0; i < n; i++) {
        nyxj_value* item = decode_value(c);
        if (!item || !nyxj_array_push(arr, item)) { nyxj_free_value(item); nyxj_free_value(arr); return NULL; }
    }
    c->depth--;
    return arr;
}

static char* decode_key(dctx* c) {
    if (!need(c, 1)) { dfail(c, "truncated map key"); return NULL; }
    uint8_t t = *c->p;
    size_t n;
    if ((t & 0xE0) == 0xA0) { c->p++; return decode_raw(c, t & 0x1F); }
    if (t == 0xD9 || t == 0xC4) { c->p++; if (!read_len(c, 1, &n)) goto bad; return decode_raw(c, n); }
    if (t == 0xDA || t == 0xC5) { c->p++; if (!read_len(c, 2, &n)) goto bad; return decode_raw(c, n); }
    if (t == 0xDB || t == 0xC6) { c->p++; if (!read_len(c, 4, &n)) goto bad; return decode_raw(c, n); }
    { /* Non-string keys (integers, ...) are converted to their JSON text */
        nyxj_value* k = decode_value(c);
        if (!k) return NULL;
        char* s = nyxj_serialize(k);
        nyxj_free_value(k);
        return s;
    }
bad:
    dfail(c, "truncated map key");
    return NULL;
}

static nyxj_value* decode_map_n(dctx* c, size_t n) {
    if (n > (size_t)(c->end - c->p)) return dfail(c, "map length exceeds the input");
    if (++c->depth > MSGPACK_MAX_DEPTH) return dfail(c, "maximum nesting depth exceeded");
    nyxj_value* obj = nyxj_new_object();
    if (!obj) return NULL;
    for (size_t i = 0; i < n; i++) {
        char* key = decode_key(c);
        if (!key) { nyxj_free_value(obj); return NULL; }
        nyxj_value* val = decode_value(c);
        if (!val || !nyxj_object_set(obj, key, val)) {
            free(key);
            nyxj_free_value(val);
            nyxj_free_value(obj);
            return NULL;
        }
        free(key);
    }
    c->depth--;
    return obj;
}

static nyxj_value* decode_value(dctx* c) {
    if (!need(c, 1)) return dfail(c, "unexpected end of data");
    uint8_t t = *c->p++;
    size_t n;
    if (t <= 0x7F) return nyxj_new_number(t);
    if (t >= 0xE0) return nyxj_new_number((int8_t)t);
    if ((t & 0xF0) == 0x80) return decode_map_n(c, t & 0x0F);
    if ((t & 0xF0) == 0x90) return decode_array_n(c, t & 0x0F);
    if ((t & 0xE0) == 0xA0) return decode_string_n(c, t & 0x1F);
    switch (t) {
        case 0xC0: return nyxj_new_null();
        case 0xC2: return nyxj_new_bool(false);
        case 0xC3: return nyxj_new_bool(true);
        case 0xC4: case 0xD9: if (!read_len(c, 1, &n)) break; return decode_string_n(c, n);
        case 0xC5: case 0xDA: if (!read_len(c, 2, &n)) break; return decode_string_n(c, n);
        case 0xC6: case 0xDB: if (!read_len(c, 4, &n)) break; return decode_string_n(c, n);
        case 0xCA: {
            if (!need(c, 4)) break;
            uint32_t bits = (uint32_t)get_be(c, 4);
            float f;
            memcpy(&f, &bits, 4);
            return nyxj_new_number(f);
        }
        case 0xCB: {
            if (!need(c, 8)) break;
            uint64_t bits = get_be(c, 8);
            double d;
            memcpy(&d, &bits, 8);
            return nyxj_new_number(d);
        }
        case 0xCC: if (!need(c, 1)) break; return nyxj_new_number((double)get_be(c, 1));
        case 0xCD: if (!need(c, 2)) break; return nyxj_new_number((double)get_be(c, 2));
        case 0xCE: if (!need(c, 4)) break; return nyxj_new_number((double)get_be(c, 4));
        case 0xCF: if (!need(c, 8)) break; return nyxj_new_number((double)get_be(c, 8));
        case 0xD0: if (!need(c, 1)) break; return nyxj_new_number((int8_t)get_be(c, 1));
        case 0xD1: if (!need(c, 2)) break; return nyxj_new_number((int16_t)get_be(c, 2));
        case 0xD2: if (!need(c, 4)) break; return nyxj_new_number((int32_t)get_be(c, 4));
        case 0xD3: if (!need(c, 8)) break; return nyxj_new_number((double)(int64_t)get_be(c, 8));
        case 0xDC: if (!read_len(c, 2, &n)) break; return decode_array_n(c, n);
        case 0xDD: if (!read_len(c, 4, &n)) break; return decode_array_n(c, n);
        case 0xDE: if (!read_len(c, 2, &n)) break; return decode_map_n(c, n);
        case 0xDF: if (!read_len(c, 4, &n)) break; return decode_map_n(c, n);
        case 0xC1: c->p--; return dfail(c, "reserved type 0xC1");
        default: c->p--; return dfail(c, "extension types are not supported");
    }
    return dfail(c, "truncated value");
}

nyxj_value* nyxj_msgpack_decode(const uint8_t* data, size_t len, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!data) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return NULL; }
    dctx c = {data, data + len, data, 0, err};
    nyxj_value* v = decode_value(&c);
    if (v && c.p != c.end) {
        nyxj_free_value(v);
        return dfail(&c, "trailing bytes after the value");
    }
    return v;
}

uint8_t* nyxj_json_to_msgpack(const char* json, size_t len, size_t* out_len, nyxj_error* err) {
    nyxj_result r = nyxj_parse_ex(json, len, NULL);
    if (!r.is_valid) { if (err) *err = r.error; return NULL; }
    uint8_t* out = nyxj_msgpack_encode(r.root, out_len);
    nyxj_free_value(r.root);
    return out;
}

char* nyxj_msgpack_to_json(const uint8_t* data, size_t len, nyxj_error* err) {
    nyxj_value* v = nyxj_msgpack_decode(data, len, err);
    if (!v) return NULL;
    char* out = nyxj_serialize(v);
    nyxj_free_value(v);
    return out;
}
