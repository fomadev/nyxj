/*
 * Streaming SAX-style (event-driven) parser.
 *
 * Feed the input chunk by chunk; events fire as soon as tokens complete.
 * Memory use is constant and decided at creation time (depth stack + one
 * token buffer), whatever the size of the stream: ideal for multi-GB files,
 * sockets and endless IoT sensor feeds (NDJSON with multi_document).
 */
#ifndef NYXJ_SAX_H
#define NYXJ_SAX_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Every callback is optional. Return false to abort parsing. */
typedef struct {
    bool (*on_object_start)(void* user);
    bool (*on_object_end)(void* user);
    bool (*on_array_start)(void* user);
    bool (*on_array_end)(void* user);
    bool (*on_key)(void* user, const char* key, size_t len);
    bool (*on_string)(void* user, const char* s, size_t len);
    bool (*on_number)(void* user, double value, const char* raw, size_t len);
    bool (*on_bool)(void* user, bool value);
    bool (*on_null)(void* user);
    bool (*on_document_end)(void* user);
} nyxj_sax_handler;

typedef struct {
    int max_depth;         /* 0 = NYXJ_DEFAULT_MAX_DEPTH */
    size_t max_token_size; /* Longest string/number in bytes, 0 = 64 KiB */
    size_t max_elements;   /* Items per container, 0 = unlimited */
    bool multi_document;   /* Accept a stream of values (NDJSON, concatenated JSON) */
} nyxj_sax_options;

typedef struct nyxj_sax nyxj_sax;

nyxj_sax*         nyxj_sax_create(const nyxj_sax_handler* handler, void* user, const nyxj_sax_options* opts);
bool              nyxj_sax_feed(nyxj_sax* sax, const char* data, size_t len);
bool              nyxj_sax_finish(nyxj_sax* sax);
const nyxj_error* nyxj_sax_error(const nyxj_sax* sax);
size_t            nyxj_sax_memory(const nyxj_sax* sax);    /* Constant footprint in bytes */
uint64_t          nyxj_sax_bytes(const nyxj_sax* sax);     /* Bytes consumed so far */
uint64_t          nyxj_sax_documents(const nyxj_sax* sax); /* Completed top-level values */
void              nyxj_sax_reset(nyxj_sax* sax);
void              nyxj_sax_destroy(nyxj_sax* sax);

/* One-shot structural validation without building a tree. */
bool nyxj_sax_validate(const char* data, size_t len, const nyxj_sax_options* opts, nyxj_error* err);

#ifdef __cplusplus
}
#endif

#endif
