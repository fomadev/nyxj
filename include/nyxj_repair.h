/*
 * Heuristic JSON repair ("fuzzy parsing").
 *
 * Turns almost-JSON into valid JSON instead of rejecting a critical
 * transaction for a cosmetic mistake: comments, single quotes, unquoted
 * keys and values, missing or trailing commas, missing colons, unclosed
 * strings and brackets, Python/JavaScript literals (True, None, undefined,
 * NaN), sloppy numbers (+1, .5, 007, 0x1F), raw control characters,
 * mismatched brackets and Markdown code fences.
 */
#ifndef NYXJ_REPAIR_H
#define NYXJ_REPAIR_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NYXJ_FIX_COMMENT        (1u << 0)
#define NYXJ_FIX_QUOTES         (1u << 1)
#define NYXJ_FIX_UNQUOTED_KEY   (1u << 2)
#define NYXJ_FIX_TRAILING_COMMA (1u << 3)
#define NYXJ_FIX_MISSING_COMMA  (1u << 4)
#define NYXJ_FIX_MISSING_COLON  (1u << 5)
#define NYXJ_FIX_UNCLOSED       (1u << 6)
#define NYXJ_FIX_LITERAL        (1u << 7)
#define NYXJ_FIX_NUMBER         (1u << 8)
#define NYXJ_FIX_ESCAPE         (1u << 9)
#define NYXJ_FIX_GARBAGE        (1u << 10)
#define NYXJ_FIX_MISSING_VALUE  (1u << 11)
#define NYXJ_FIX_TRAILING_DATA  (1u << 12)
#define NYXJ_FIX_BRACKET        (1u << 13)
#define NYXJ_FIX_BOM            (1u << 14)
#define NYXJ_FIX_CODE_FENCE     (1u << 15)
#define NYXJ_FIX_KIND_COUNT     16

typedef struct {
    int fixes;      /* Number of individual corrections */
    unsigned kinds; /* Bitmask of NYXJ_FIX_* */
} nyxj_repair_report;

/* Returns repaired, compact JSON (caller frees) or NULL when nothing is salvageable. */
char* nyxj_repair(const char* text, size_t len, size_t* out_len, nyxj_repair_report* report);

/* Repairs then parses. */
nyxj_result nyxj_parse_lenient(const char* text, size_t len, nyxj_repair_report* report);

const char* nyxj_repair_kind_name(unsigned kind_bit); /* "trailing_comma" */
nyxj_value* nyxj_repair_report_to_value(const nyxj_repair_report* report);

#ifdef __cplusplus
}
#endif

#endif
