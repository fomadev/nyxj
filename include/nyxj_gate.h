/*
 * The Guardian: a sanitizer that inspects raw network buffers before they
 * reach any parser. One pass, no allocation per value, constant memory.
 *
 * Rejects oversized payloads, invalid UTF-8, NUL bytes, deeply nested
 * structures ([[[[...]]]] JSON Denial-of-Service), huge strings, numbers or
 * containers, and malformed JSON. Used by the server middleware and by the
 * Safe-Gate reverse proxy mode.
 */
#ifndef NYXJ_GATE_H
#define NYXJ_GATE_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t max_size;          /* Total bytes, 0 = unlimited */
    int max_depth;            /* 0 = NYXJ_DEFAULT_MAX_DEPTH */
    size_t max_string_length; /* Per string/number token, 0 = 64 KiB */
    size_t max_elements;      /* Per container, 0 = unlimited */
    bool require_object_root; /* Typical API bodies must be objects */
    bool allow_empty;         /* An empty body passes */
} nyxj_gate_policy;

typedef struct {
    bool allowed;
    nyxj_error error;
    const char* reason; /* Short machine-friendly reason, e.g. "depth_limit" */
    size_t body_offset; /* Payload start after an optional UTF-8 BOM */
} nyxj_gate_verdict;

nyxj_gate_policy nyxj_gate_policy_default(void); /* 1 MiB, depth 64, 64 KiB strings, 10k elements */
nyxj_gate_policy nyxj_gate_policy_strict(void);  /* 64 KiB, depth 16, 4 KiB strings, 1k elements, object root */

bool nyxj_gate_check(const char* buf, size_t len, const nyxj_gate_policy* policy, nyxj_gate_verdict* verdict);

#ifdef __cplusplus
}
#endif

#endif
