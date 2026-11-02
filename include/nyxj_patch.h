/*
 * Differential state tracking: JSON Pointer (RFC 6901), JSON Patch
 * (RFC 6902) and JSON Merge Patch (RFC 7386).
 *
 * Instead of resending a whole document, compute the delta between the
 * previous and the current state and ship only that.
 */
#ifndef NYXJ_PATCH_H
#define NYXJ_PATCH_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "/store/books/0/title" -> borrowed pointer, NULL when absent. "" is the root. */
nyxj_value* nyxj_pointer_get(const nyxj_value* root, const char* pointer);

/* RFC 6902: returns an array of operations turning `from` into `to`. */
nyxj_value* nyxj_diff(const nyxj_value* from, const nyxj_value* to);

/*
 * Applies an RFC 6902 patch atomically: on failure *doc is left untouched.
 * Supports add, remove, replace, move, copy and test.
 */
bool nyxj_patch_apply(nyxj_value** doc, const nyxj_value* patch, nyxj_error* err);

/* RFC 7386: consumes `target`, returns the merged document. */
nyxj_value* nyxj_merge_patch(nyxj_value* target, const nyxj_value* patch);

/* Smallest merge patch turning `from` into `to` (null marks deletions). */
nyxj_value* nyxj_merge_diff(const nyxj_value* from, const nyxj_value* to);

#ifdef __cplusplus
}
#endif

#endif
