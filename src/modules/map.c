#include "../../include/nyxj_map.h"
#include "../common/buf.h"
#include "../core/internal.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* --- JSON -> struct --- */

static bool map_fields(const nyxj_value* obj, const nyxj_field* fields, size_t count, unsigned char* base,
                       const char* prefix, nyxj_error* err);

static bool is_integral(double d) {
    return d > -9.2e18 && d < 9.2e18 && d == (double)(long long)d;
}

static bool map_one(const nyxj_value* v, const nyxj_field* f, unsigned char* dst, const char* path, nyxj_error* err) {
    switch (f->type) {
        case NYXJ_MAP_INT:
        case NYXJ_MAP_INT64: {
            if (v->type != NYXJ_NUMBER || !is_integral(v->as.number)) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected an integer, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            double d = v->as.number;
            if (f->type == NYXJ_MAP_INT) {
                if (d < INT_MIN || d > INT_MAX) {
                    nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: integer out of range", path);
                    return false;
                }
                int x = (int)d;
                memcpy(dst, &x, sizeof(x));
            } else {
                int64_t x = (int64_t)d;
                memcpy(dst, &x, sizeof(x));
            }
            return true;
        }
        case NYXJ_MAP_DOUBLE:
        case NYXJ_MAP_FLOAT:
            if (v->type != NYXJ_NUMBER) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected a number, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            if (f->type == NYXJ_MAP_DOUBLE) memcpy(dst, &v->as.number, sizeof(double));
            else { float x = (float)v->as.number; memcpy(dst, &x, sizeof(x)); }
            return true;
        case NYXJ_MAP_BOOL:
            if (v->type != NYXJ_BOOL) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected a boolean, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            memcpy(dst, &v->as.boolean, sizeof(bool));
            return true;
        case NYXJ_MAP_STRING: {
            if (v->type != NYXJ_STRING) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected a string, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            char* old;
            memcpy(&old, dst, sizeof(char*));
            free(old);
            char* s = nyxj_strdup(v->as.string);
            if (!s) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
            memcpy(dst, &s, sizeof(char*));
            return true;
        }
        case NYXJ_MAP_STRBUF: {
            if (v->type != NYXJ_STRING) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected a string, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            size_t n = strlen(v->as.string);
            if (n + 1 > f->size) {
                nyxj_error_set(err, NYXJ_ERR_STRING_LIMIT, "%s: string of %zu bytes does not fit in %zu bytes", path, n, f->size - 1);
                return false;
            }
            memcpy(dst, v->as.string, n + 1);
            return true;
        }
        case NYXJ_MAP_OBJECT:
            if (v->type != NYXJ_OBJECT) {
                nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "%s: expected an object, got %s", path, nyxj_type_name(v->type));
                return false;
            }
            return map_fields(v, f->fields, f->field_count, dst, path, err);
        case NYXJ_MAP_VALUE: {
            nyxj_value* old;
            memcpy(&old, dst, sizeof(old));
            nyxj_free_value(old);
            nyxj_value* c = nyxj_clone(v);
            if (!c) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
            memcpy(dst, &c, sizeof(c));
            return true;
        }
    }
    nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, "%s: unknown field type", path);
    return false;
}

static bool map_fields(const nyxj_value* obj, const nyxj_field* fields, size_t count, unsigned char* base,
                       const char* prefix, nyxj_error* err) {
    for (size_t i = 0; i < count; i++) {
        const nyxj_field* f = &fields[i];
        char path[192];
        if (prefix && *prefix) snprintf(path, sizeof(path), "%s.%s", prefix, f->key);
        else snprintf(path, sizeof(path), "%s", f->key);
        const nyxj_value* v = nyxj_get_member(obj, f->key);
        if (!v || (v->type == NYXJ_NULL && f->type != NYXJ_MAP_VALUE)) {
            if (f->required) {
                nyxj_error_set(err, NYXJ_ERR_MISSING_FIELD, "%s: required field is missing", path);
                return false;
            }
            continue;
        }
        if (!map_one(v, f, base + f->offset, path, err)) return false;
    }
    return true;
}

bool nyxj_map_to_struct(const nyxj_value* obj, const nyxj_field* fields, size_t count, void* out, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!obj || !fields || !out) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    if (obj->type != NYXJ_OBJECT) {
        nyxj_error_set(err, NYXJ_ERR_TYPE_MISMATCH, "root: expected an object, got %s", nyxj_type_name(obj->type));
        return false;
    }
    return map_fields(obj, fields, count, out, "", err);
}

bool nyxj_map_parse(const char* json, const nyxj_field* fields, size_t count, void* out, nyxj_error* err) {
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) {
        if (err) *err = r.error;
        return false;
    }
    bool ok = nyxj_map_to_struct(r.root, fields, count, out, err);
    nyxj_free_value(r.root);
    return ok;
}

/* --- struct -> JSON --- */

nyxj_value* nyxj_struct_to_value(const void* in, const nyxj_field* fields, size_t count) {
    if (!in || !fields) return NULL;
    const unsigned char* base = in;
    nyxj_value* obj = nyxj_new_object();
    if (!obj) return NULL;
    for (size_t i = 0; i < count; i++) {
        const nyxj_field* f = &fields[i];
        const unsigned char* src = base + f->offset;
        nyxj_value* v = NULL;
        switch (f->type) {
            case NYXJ_MAP_INT: { int x; memcpy(&x, src, sizeof(x)); v = nyxj_new_number(x); break; }
            case NYXJ_MAP_INT64: { int64_t x; memcpy(&x, src, sizeof(x)); v = nyxj_new_number((double)x); break; }
            case NYXJ_MAP_DOUBLE: { double x; memcpy(&x, src, sizeof(x)); v = nyxj_new_number(x); break; }
            case NYXJ_MAP_FLOAT: { float x; memcpy(&x, src, sizeof(x)); v = nyxj_new_number(x); break; }
            case NYXJ_MAP_BOOL: { bool x; memcpy(&x, src, sizeof(x)); v = nyxj_new_bool(x); break; }
            case NYXJ_MAP_STRING: {
                char* s;
                memcpy(&s, src, sizeof(s));
                v = s ? nyxj_new_string(s) : nyxj_new_null();
                break;
            }
            case NYXJ_MAP_STRBUF: v = nyxj_new_stringn((const char*)src, strnlen((const char*)src, f->size)); break;
            case NYXJ_MAP_OBJECT: v = nyxj_struct_to_value(src, f->fields, f->field_count); break;
            case NYXJ_MAP_VALUE: {
                nyxj_value* x;
                memcpy(&x, src, sizeof(x));
                v = x ? nyxj_clone(x) : nyxj_new_null();
                break;
            }
        }
        if (!v || !nyxj_object_set(obj, f->key, v)) {
            nyxj_free_value(v);
            nyxj_free_value(obj);
            return NULL;
        }
    }
    return obj;
}

char* nyxj_struct_to_json(const void* in, const nyxj_field* fields, size_t count, int indent) {
    nyxj_value* v = nyxj_struct_to_value(in, fields, count);
    if (!v) return NULL;
    char* out = indent > 0 ? nyxj_serialize_pretty(v, indent) : nyxj_serialize(v);
    nyxj_free_value(v);
    return out;
}

void nyxj_map_free(void* s, const nyxj_field* fields, size_t count) {
    if (!s || !fields) return;
    unsigned char* base = s;
    for (size_t i = 0; i < count; i++) {
        const nyxj_field* f = &fields[i];
        unsigned char* p = base + f->offset;
        if (f->type == NYXJ_MAP_STRING) {
            char* str;
            memcpy(&str, p, sizeof(str));
            free(str);
            str = NULL;
            memcpy(p, &str, sizeof(str));
        } else if (f->type == NYXJ_MAP_VALUE) {
            nyxj_value* v;
            memcpy(&v, p, sizeof(v));
            nyxj_free_value(v);
            v = NULL;
            memcpy(p, &v, sizeof(v));
        } else if (f->type == NYXJ_MAP_OBJECT) {
            nyxj_map_free(p, f->fields, f->field_count);
        }
    }
}

/* --- Code generator ("the Unifier") --- */

static void c_identifier(const char* key, char* out, size_t n) {
    size_t k = 0;
    if (!isalpha((unsigned char)key[0]) && key[0] != '_' && k + 1 < n) out[k++] = '_';
    for (const char* p = key; *p && k + 1 < n; p++)
        out[k++] = (isalnum((unsigned char)*p) || *p == '_') ? *p : '_';
    out[k] = '\0';
    static const char* reserved[] = {"int", "char", "float", "double", "long", "short", "signed", "unsigned",
                                     "void", "struct", "union", "enum", "if", "else", "for", "while", "do",
                                     "switch", "case", "default", "return", "break", "continue", "goto",
                                     "static", "const", "volatile", "extern", "auto", "register", "sizeof",
                                     "typedef", "bool", "inline", "restrict"};
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcmp(out, reserved[i]) == 0 && k + 2 < n) { out[k++] = '_'; out[k] = '\0'; break; }
    }
}

static void type_name_for(const char* parent, const char* key, char* out, size_t n) {
    char id[96];
    c_identifier(key, id, sizeof(id));
    if (id[0] >= 'a' && id[0] <= 'z') id[0] = (char)(id[0] - 32);
    snprintf(out, n, "%s%s", parent, id);
}

static void gen_struct(nyxj_buf* decls, nyxj_buf* tables, const nyxj_value* obj, const char* name) {
    /* Nested objects first so their types are declared before use. */
    for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next) {
        if (p->value->type == NYXJ_OBJECT) {
            char sub[160];
            type_name_for(name, p->key, sub, sizeof(sub));
            gen_struct(decls, tables, p->value, sub);
        }
    }
    nyxj_buf_printf(decls, "typedef struct {\n");
    for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next) {
        char id[96];
        c_identifier(p->key, id, sizeof(id));
        const nyxj_value* v = p->value;
        switch (v->type) {
            case NYXJ_BOOL: nyxj_buf_printf(decls, "    bool %s;\n", id); break;
            case NYXJ_NUMBER:
                if (is_integral(v->as.number) && v->as.number >= INT_MIN && v->as.number <= INT_MAX)
                    nyxj_buf_printf(decls, "    int %s;\n", id);
                else if (is_integral(v->as.number))
                    nyxj_buf_printf(decls, "    int64_t %s;\n", id);
                else
                    nyxj_buf_printf(decls, "    double %s;\n", id);
                break;
            case NYXJ_STRING: nyxj_buf_printf(decls, "    char* %s;\n", id); break;
            case NYXJ_OBJECT: {
                char sub[160];
                type_name_for(name, p->key, sub, sizeof(sub));
                nyxj_buf_printf(decls, "    %s %s;\n", sub, id);
                break;
            }
            default: nyxj_buf_printf(decls, "    nyxj_value* %s; /* %s */\n", id, nyxj_type_name(v->type)); break;
        }
    }
    nyxj_buf_printf(decls, "} %s;\n\n", name);

    nyxj_buf_printf(tables, "static const nyxj_field %s_fields[] = {\n", name);
    for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next) {
        char id[96];
        c_identifier(p->key, id, sizeof(id));
        const nyxj_value* v = p->value;
        const char* macro = "VALUE";
        switch (v->type) {
            case NYXJ_BOOL: macro = "BOOL"; break;
            case NYXJ_NUMBER:
                macro = !is_integral(v->as.number) ? "DOUBLE"
                        : (v->as.number >= INT_MIN && v->as.number <= INT_MAX) ? "INT" : "INT64";
                break;
            case NYXJ_STRING: macro = "STRING"; break;
            case NYXJ_OBJECT: macro = "OBJECT"; break;
            default: break;
        }
        nyxj_buf_printf(tables, "    NYXJ_FIELD_%s(%s, %s, ", macro, name, id);
        nyxj_buf_json_string(tables, p->key, false);
        if (v->type == NYXJ_OBJECT) {
            char sub[160];
            type_name_for(name, p->key, sub, sizeof(sub));
            nyxj_buf_printf(tables, ", true, %s_fields),\n", sub);
        } else {
            nyxj_buf_printf(tables, ", true),\n");
        }
    }
    nyxj_buf_printf(tables, "};\n\n");
}

char* nyxj_generate_struct(const nyxj_value* sample, const char* type_name) {
    if (!sample) return NULL;
    const nyxj_value* obj = sample;
    if (obj->type == NYXJ_ARRAY && obj->as.array.count > 0) obj = obj->as.array.items[0];
    if (obj->type != NYXJ_OBJECT) return NULL;
    char name[96];
    c_identifier(type_name && *type_name ? type_name : "Document", name, sizeof(name));
    nyxj_buf decls, tables, out;
    nyxj_buf_init(&decls, 512);
    nyxj_buf_init(&tables, 512);
    nyxj_buf_init(&out, 1024);
    gen_struct(&decls, &tables, obj, name);
    nyxj_buf_printf(&out, "/* Generated by NyxJ %s (nyxj gen-struct). */\n", NYXJ_VERSION);
    nyxj_buf_puts(&out, "#include <stdbool.h>\n#include <stdint.h>\n#include \"nyxj_map.h\"\n\n");
    if (decls.data) nyxj_buf_puts(&out, decls.data);
    if (tables.data) nyxj_buf_puts(&out, tables.data);
    nyxj_buf_printf(&out,
                    "/* Usage:\n *   %s doc = {0};\n *   nyxj_map_parse(json, %s_fields, NYXJ_FIELDS_COUNT(%s_fields), &doc, &err);\n"
                    " *   nyxj_map_free(&doc, %s_fields, NYXJ_FIELDS_COUNT(%s_fields));\n */\n",
                    name, name, name, name, name);
    nyxj_buf_free(&decls);
    nyxj_buf_free(&tables);
    return nyxj_buf_detach(&out, NULL);
}
