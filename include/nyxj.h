/*
 * NyxJ Engine - high-performance JSON engine and micro-service framework in pure C.
 *
 * Core API: values, parsing, serialization, errors, dot-path access and the
 * arena allocator. Every module header (nyxj_*.h) builds on these types.
 */
#ifndef NYXJ_H
#define NYXJ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Version --- */

#define NYXJ_VERSION_MAJOR 2
#define NYXJ_VERSION_MINOR 0
#define NYXJ_VERSION_PATCH 0
#define NYXJ_VERSION "2.0.0"

/* --- Default limits (JSON Denial-of-Service protection) --- */

#define NYXJ_DEFAULT_MAX_DEPTH 512

/* --- Error codes --- */

typedef enum {
    NYXJ_OK = 0,
    NYXJ_ERR_NULL_INPUT,
    NYXJ_ERR_OUT_OF_MEMORY,
    NYXJ_ERR_UNEXPECTED_EOF,
    NYXJ_ERR_UNEXPECTED_CHAR,
    NYXJ_ERR_INVALID_LITERAL,
    NYXJ_ERR_INVALID_NUMBER,
    NYXJ_ERR_INVALID_STRING,
    NYXJ_ERR_INVALID_ESCAPE,
    NYXJ_ERR_INVALID_UNICODE,
    NYXJ_ERR_INVALID_UTF8,
    NYXJ_ERR_CONTROL_CHAR,
    NYXJ_ERR_TRAILING_DATA,
    NYXJ_ERR_DUPLICATE_KEY,
    NYXJ_ERR_DEPTH_LIMIT,
    NYXJ_ERR_SIZE_LIMIT,
    NYXJ_ERR_STRING_LIMIT,
    NYXJ_ERR_ELEMENT_LIMIT,
    NYXJ_ERR_TYPE_MISMATCH,
    NYXJ_ERR_NOT_FOUND,
    NYXJ_ERR_MISSING_FIELD,
    NYXJ_ERR_SCHEMA_VIOLATION,
    NYXJ_ERR_INVALID_SCHEMA,
    NYXJ_ERR_INVALID_QUERY,
    NYXJ_ERR_INVALID_POINTER,
    NYXJ_ERR_PATCH_FAILED,
    NYXJ_ERR_TEST_FAILED,
    NYXJ_ERR_MSGPACK,
    NYXJ_ERR_INTEGRITY,
    NYXJ_ERR_IO,
    NYXJ_ERR_NETWORK,
    NYXJ_ERR_TIMEOUT,
    NYXJ_ERR_TLS,
    NYXJ_ERR_QUEUE_FULL,
    NYXJ_ERR_PLUGIN,
    NYXJ_ERR_UNSUPPORTED,
    NYXJ_ERR_INVALID_ARGUMENT,
    NYXJ_ERR_COUNT
} nyxj_error_code;

/* Detailed error: what failed and exactly where in the input. */
typedef struct {
    nyxj_error_code code;
    size_t offset;      /* Byte offset in the input (0 when not applicable) */
    int line;           /* 1-based line, 0 when not applicable */
    int column;         /* 1-based column (in bytes) */
    char message[160];  /* Human-readable description */
} nyxj_error;

const char* nyxj_strerror(nyxj_error_code code);   /* "maximum nesting depth exceeded" */
const char* nyxj_error_name(nyxj_error_code code); /* "DEPTH_LIMIT" */
void        nyxj_error_clear(nyxj_error* err);
void        nyxj_error_set(nyxj_error* err, nyxj_error_code code, const char* fmt, ...);

/* --- Values --- */

typedef enum {
    NYXJ_NULL,
    NYXJ_BOOL,
    NYXJ_NUMBER,
    NYXJ_STRING,
    NYXJ_ARRAY,
    NYXJ_OBJECT
} nyxj_type;

typedef struct nyxj_value nyxj_value;
typedef struct nyxj_arena nyxj_arena;

typedef struct nyxj_pair {
    char* key;
    nyxj_value* value;
    struct nyxj_pair* next;
} nyxj_pair;

struct nyxj_value {
    nyxj_type type;
    unsigned flags; /* Internal (arena ownership). Build values with the nyxj_new_* API. */
    union {
        bool boolean;
        double number;
        char* string;
        struct {
            nyxj_value** items;
            int count;
            int capacity;
        } array;
        struct {
            nyxj_pair* pairs;
            int count;
        } object;
    } as;
};

/* Universal result: the data and a precise error, like Result<T, E>. */
typedef struct {
    bool is_valid;
    const char* error_msg; /* Static description, NULL on success (1.x compatible) */
    nyxj_value* root;
    nyxj_error error;
} nyxj_result;

/* --- Parsing --- */

#define NYXJ_PARSE_ALLOW_COMMENTS        (1u << 0) /* Accept // and block comments */
#define NYXJ_PARSE_ALLOW_TRAILING_COMMAS (1u << 1) /* Accept [1,2,] and {"a":1,} */
#define NYXJ_PARSE_REJECT_DUPLICATE_KEYS (1u << 2) /* Fail on {"a":1,"a":2} */
#define NYXJ_PARSE_RELAXED (NYXJ_PARSE_ALLOW_COMMENTS | NYXJ_PARSE_ALLOW_TRAILING_COMMAS)

typedef struct {
    unsigned flags;
    int max_depth;            /* 0 = NYXJ_DEFAULT_MAX_DEPTH */
    size_t max_input_size;    /* Bytes, 0 = unlimited */
    size_t max_string_length; /* Bytes per string, 0 = unlimited */
    size_t max_elements;      /* Items per array/object, 0 = unlimited */
    nyxj_arena* arena;        /* Allocate the tree in this arena (read-only tree,
                                 released with the arena, never nyxj_free_value) */
} nyxj_parse_options;

nyxj_parse_options nyxj_parse_options_default(void);

nyxj_result nyxj_parse(const char* json_str);
nyxj_result nyxj_parse_n(const char* json, size_t len);
nyxj_result nyxj_parse_ex(const char* json, size_t len, const nyxj_parse_options* opts);
nyxj_value* nyxj_parse_file(const char* path, const nyxj_parse_options* opts, nyxj_error* err);

void nyxj_free_value(nyxj_value* v);
void nyxj_result_free(nyxj_result* r);

/* Reads a whole file into a NUL-terminated buffer (caller frees). */
char* nyxj_read_file(const char* path, size_t* out_len, nyxj_error* err);

/* --- Builder API (heap values, freed with nyxj_free_value) --- */

nyxj_value* nyxj_new_null(void);
nyxj_value* nyxj_new_bool(bool b);
nyxj_value* nyxj_new_number(double n);
nyxj_value* nyxj_new_string(const char* s);
nyxj_value* nyxj_new_stringn(const char* s, size_t len);
nyxj_value* nyxj_new_array(void);
nyxj_value* nyxj_new_object(void);

bool        nyxj_array_push(nyxj_value* arr, nyxj_value* item);              /* Takes ownership */
bool        nyxj_array_insert(nyxj_value* arr, int index, nyxj_value* item); /* Takes ownership */
nyxj_value* nyxj_array_get(const nyxj_value* arr, int index);
nyxj_value* nyxj_array_detach(nyxj_value* arr, int index);                   /* Caller owns result */
bool        nyxj_array_remove(nyxj_value* arr, int index);

bool        nyxj_object_set(nyxj_value* obj, const char* key, nyxj_value* val); /* Replaces, takes ownership */
nyxj_value* nyxj_object_detach(nyxj_value* obj, const char* key);               /* Caller owns result */
bool        nyxj_object_remove(nyxj_value* obj, const char* key);

/* Convenience setters (create the child value). */
bool nyxj_object_set_string(nyxj_value* obj, const char* key, const char* s);
bool nyxj_object_set_number(nyxj_value* obj, const char* key, double n);
bool nyxj_object_set_bool(nyxj_value* obj, const char* key, bool b);
bool nyxj_object_set_null(nyxj_value* obj, const char* key);

nyxj_value* nyxj_clone(const nyxj_value* v);
bool        nyxj_equals(const nyxj_value* a, const nyxj_value* b);
int         nyxj_size(const nyxj_value* v); /* Items, members, or string bytes */
const char* nyxj_type_name(nyxj_type type);

/* --- Getter API --- */

nyxj_value* nyxj_get_member(const nyxj_value* obj, const char* key);
double      nyxj_get_number(const nyxj_value* obj, const char* key);
int64_t     nyxj_get_int(const nyxj_value* obj, const char* key);
const char* nyxj_get_string(const nyxj_value* obj, const char* key);
bool        nyxj_get_bool(const nyxj_value* obj, const char* key);

/* Dot-path access in one line: "user.address.city", "items[2].name". */
nyxj_value* nyxj_get_path(const nyxj_value* root, const char* path);
const char* nyxj_path_string(const nyxj_value* root, const char* path, const char* fallback);
double      nyxj_path_number(const nyxj_value* root, const char* path, double fallback);
bool        nyxj_path_bool(const nyxj_value* root, const char* path, bool fallback);

/* --- Generator API --- */

typedef struct {
    int indent;         /* 0 = compact output, N = pretty-print with N spaces */
    bool sort_keys;     /* Canonical output with keys in byte order */
    bool escape_unicode;/* Escape non-ASCII characters as \uXXXX */
} nyxj_serialize_options;

char* nyxj_serialize(const nyxj_value* v);                    /* Compact, caller frees */
char* nyxj_serialize_pretty(const nyxj_value* v, int indent); /* Pretty, caller frees */
char* nyxj_serialize_ex(const nyxj_value* v, const nyxj_serialize_options* opts, size_t* out_len);

/* --- Arena allocator (memory pool, no fragmentation, O(1) release) --- */

nyxj_arena* nyxj_arena_create(size_t block_size);
void*       nyxj_arena_alloc(nyxj_arena* arena, size_t size);
char*       nyxj_arena_strndup(nyxj_arena* arena, const char* s, size_t len);
void        nyxj_arena_reset(nyxj_arena* arena);
size_t      nyxj_arena_used(const nyxj_arena* arena);
void        nyxj_arena_destroy(nyxj_arena* arena);

/* --- Library information --- */

const char* nyxj_version(void);
const char* nyxj_simd_backend(void); /* "sse2", "neon" or "scalar" */
const char* nyxj_build_features(void);

#ifdef __cplusplus
}
#endif

#endif
