/*
 * Fail-safe processing for critical systems (medical, industrial, automotive).
 *
 * - Shadow execution: parse the same input twice (on two threads when
 *   available) and compare the trees before trusting the result, so a
 *   bit-flip or a corrupted buffer is reported instead of propagated.
 * - Triple modular redundancy (TMR): keep three replicas of a document,
 *   read by majority vote and repair the corrupted replica in place.
 * - Structural checksums (FNV-1a 64) for integrity verification.
 */
#ifndef NYXJ_RELIABILITY_H
#define NYXJ_RELIABILITY_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Order-sensitive structural checksum of a tree. */
uint64_t nyxj_checksum(const nyxj_value* v);

/* Parses twice and compares; fails with NYXJ_ERR_INTEGRITY on mismatch. */
nyxj_result nyxj_parse_verified(const char* json, size_t len, const nyxj_parse_options* opts);

typedef struct {
    nyxj_value* replica[3];
    uint64_t repairs; /* Replicas repaired so far */
} nyxj_tmr;

nyxj_tmr*         nyxj_tmr_create(const nyxj_value* v);
const nyxj_value* nyxj_tmr_read(nyxj_tmr* tmr, bool* repaired); /* NULL when no majority exists */
bool              nyxj_tmr_write(nyxj_tmr* tmr, const nyxj_value* v);
void              nyxj_tmr_free(nyxj_tmr* tmr);

#ifdef __cplusplus
}
#endif

#endif
