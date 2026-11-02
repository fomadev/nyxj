/*
 * Zero-copy, non-destructive JSON view.
 *
 * The input buffer is validated and indexed into a flat token array whose
 * entries point back into the original bytes: no malloc() and no memcpy()
 * per string. Pass a caller-owned token array to parse with zero heap
 * allocations at all.
 */
#ifndef NYXJ_VIEW_H
#define NYXJ_VIEW_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NYXJ_TOK_NONE = 0,
    NYXJ_TOK_OBJECT,
    NYXJ_TOK_ARRAY,
    NYXJ_TOK_STRING,
    NYXJ_TOK_NUMBER,
    NYXJ_TOK_TRUE,
    NYXJ_TOK_FALSE,
    NYXJ_TOK_NULL
} nyxj_tok_type;

typedef struct {
    nyxj_tok_type type;
    uint32_t start; /* Byte offset (strings: first byte after the opening quote) */
    uint32_t end;   /* Exclusive byte offset (strings: the closing quote) */
    uint32_t size;  /* Members (object) or items (array) */
    uint32_t next;  /* Index of the first token after this subtree */
    bool escaped;   /* String contains escape sequences */
} nyxj_tok;

#define NYXJ_VIEW_NONE ((size_t)-1)

typedef struct {
    const char* json;
    size_t len;
    nyxj_tok* tokens;
    size_t count;
    size_t capacity;
    bool owns_tokens;
    int max_depth;
} nyxj_view;

/*
 * Indexes `json`. With tokens == NULL the token array is heap-allocated and
 * grows as needed (release with nyxj_view_free). With a caller array the
 * parse fails with NYXJ_ERR_ELEMENT_LIMIT when it is too small.
 */
bool nyxj_view_parse(nyxj_view* view, const char* json, size_t len, nyxj_tok* tokens, size_t capacity,
                     nyxj_error* err);
void nyxj_view_free(nyxj_view* view);

size_t      nyxj_view_find(const nyxj_view* view, size_t object_tok, const char* key);
size_t      nyxj_view_index(const nyxj_view* view, size_t array_tok, size_t i);
size_t      nyxj_view_path(const nyxj_view* view, const char* path); /* "a.b[2].c" from the root */
bool        nyxj_view_eq(const nyxj_view* view, size_t tok, const char* s);
const char* nyxj_view_raw(const nyxj_view* view, size_t tok, size_t* len);
double      nyxj_view_number(const nyxj_view* view, size_t tok);
bool        nyxj_view_bool(const nyxj_view* view, size_t tok);
size_t      nyxj_view_string(const nyxj_view* view, size_t tok, char* out, size_t out_size); /* Decoded length */
nyxj_value* nyxj_view_to_value(const nyxj_view* view, size_t tok); /* Materializes a subtree */

#ifdef __cplusplus
}
#endif

#endif
