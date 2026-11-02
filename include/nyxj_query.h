/*
 * NyxQuery - JSONPath queries evaluated natively on NyxJ trees.
 *
 *   $.store.book[0].title          child and index access
 *   $.store.book[-1]               negative index (from the end)
 *   $.store.book[0:2], [::-1]      slices with optional step
 *   $.store.*, $.store.book[*]     wildcards
 *   $..price                       recursive descent
 *   $['store']['book'], [0,2]      bracket notation and unions
 *   $..book[?(@.price < 10)]       filters: == != < <= > >=, &&, ||, !,
 *   $..book[?(@.isbn)]             existence tests, ( ) grouping and $-paths
 */
#ifndef NYXJ_QUERY_H
#define NYXJ_QUERY_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nyxj_compiled_query nyxj_compiled_query;

/* Return false to stop the iteration. */
typedef bool (*nyxj_query_fn)(const nyxj_value* match, void* user);

nyxj_compiled_query* nyxj_query_compile(const char* path, nyxj_error* err);
int                  nyxj_query_exec(const nyxj_compiled_query* q, const nyxj_value* root, nyxj_query_fn fn, void* user);
void                 nyxj_query_free(nyxj_compiled_query* q);

/* One-shot helpers. nyxj_query returns a new array of copies (NULL on error). */
nyxj_value*       nyxj_query(const nyxj_value* root, const char* path, nyxj_error* err);
const nyxj_value* nyxj_query_first(const nyxj_value* root, const char* path, nyxj_error* err);
int               nyxj_query_count(const nyxj_value* root, const char* path, nyxj_error* err);

#ifdef __cplusplus
}
#endif

#endif
