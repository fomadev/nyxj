/*
 * JSON <-> C struct mapping (reflection-style binding).
 *
 *   typedef struct { int id; char* name; double score; bool active; } User;
 *   static const nyxj_field user_fields[] = {
 *       NYXJ_FIELD_INT(User, id, "id", true),
 *       NYXJ_FIELD_STRING(User, name, "name", true),
 *       NYXJ_FIELD_DOUBLE(User, score, "score", false),
 *       NYXJ_FIELD_BOOL(User, active, "active", false),
 *   };
 *   User u = {0};
 *   nyxj_map_parse(json, user_fields, NYXJ_FIELDS_COUNT(user_fields), &u, &err);
 */
#ifndef NYXJ_MAP_H
#define NYXJ_MAP_H

#include <stddef.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NYXJ_MAP_INT,    /* int */
    NYXJ_MAP_INT64,  /* int64_t */
    NYXJ_MAP_DOUBLE, /* double */
    NYXJ_MAP_FLOAT,  /* float */
    NYXJ_MAP_BOOL,   /* bool */
    NYXJ_MAP_STRING, /* char* (heap copy, released by nyxj_map_free) */
    NYXJ_MAP_STRBUF, /* char[N] fixed buffer (truncation is an error) */
    NYXJ_MAP_OBJECT, /* Nested struct described by a sub-field table */
    NYXJ_MAP_VALUE   /* nyxj_value* deep copy (arrays, free-form data) */
} nyxj_map_type;

typedef struct nyxj_field {
    const char* key;
    nyxj_map_type type;
    size_t offset;
    size_t size;
    bool required;
    const struct nyxj_field* fields; /* NYXJ_MAP_OBJECT only */
    size_t field_count;
} nyxj_field;

#define NYXJ_FIELDS_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define NYXJ_FIELD_(T, m, key, kind, req) {key, kind, offsetof(T, m), sizeof(((T*)0)->m), req, NULL, 0}

#define NYXJ_FIELD_INT(T, m, key, req)    NYXJ_FIELD_(T, m, key, NYXJ_MAP_INT, req)
#define NYXJ_FIELD_INT64(T, m, key, req)  NYXJ_FIELD_(T, m, key, NYXJ_MAP_INT64, req)
#define NYXJ_FIELD_DOUBLE(T, m, key, req) NYXJ_FIELD_(T, m, key, NYXJ_MAP_DOUBLE, req)
#define NYXJ_FIELD_FLOAT(T, m, key, req)  NYXJ_FIELD_(T, m, key, NYXJ_MAP_FLOAT, req)
#define NYXJ_FIELD_BOOL(T, m, key, req)   NYXJ_FIELD_(T, m, key, NYXJ_MAP_BOOL, req)
#define NYXJ_FIELD_STRING(T, m, key, req) NYXJ_FIELD_(T, m, key, NYXJ_MAP_STRING, req)
#define NYXJ_FIELD_STRBUF(T, m, key, req) NYXJ_FIELD_(T, m, key, NYXJ_MAP_STRBUF, req)
#define NYXJ_FIELD_VALUE(T, m, key, req)  NYXJ_FIELD_(T, m, key, NYXJ_MAP_VALUE, req)
#define NYXJ_FIELD_OBJECT(T, m, key, req, sub) \
    {key, NYXJ_MAP_OBJECT, offsetof(T, m), sizeof(((T*)0)->m), req, sub, NYXJ_FIELDS_COUNT(sub)}

/* Fills `out` from a JSON object. On failure err names the faulty field path. */
bool nyxj_map_to_struct(const nyxj_value* obj, const nyxj_field* fields, size_t count, void* out,
                        nyxj_error* err);

/* Parses `json` and maps it in one call. */
bool nyxj_map_parse(const char* json, const nyxj_field* fields, size_t count, void* out, nyxj_error* err);

/* Builds a JSON object from a struct (inverse mapping). */
nyxj_value* nyxj_struct_to_value(const void* in, const nyxj_field* fields, size_t count);
char*       nyxj_struct_to_json(const void* in, const nyxj_field* fields, size_t count, int indent);

/* Releases heap members (NYXJ_MAP_STRING / NYXJ_MAP_VALUE) of a mapped struct. */
void nyxj_map_free(void* s, const nyxj_field* fields, size_t count);

/*
 * Code generator ("the Unifier"): emits C struct declarations and their
 * field tables from a sample JSON document. Caller frees the returned code.
 */
char* nyxj_generate_struct(const nyxj_value* sample, const char* type_name);

#ifdef __cplusplus
}
#endif

#endif
