/*
 * JSON Schema validation (practical subset of drafts 4 to 2020-12).
 *
 * Supported keywords: type, enum, const, $ref (local), allOf, anyOf, oneOf,
 * not, if/then/else, properties, required, additionalProperties,
 * minProperties, maxProperties, items, prefixItems, contains, minItems,
 * maxItems, uniqueItems, minimum, maximum, exclusiveMinimum,
 * exclusiveMaximum, multipleOf, minLength, maxLength and format
 * (email, uuid, ipv4, date, date-time). Unknown keywords are ignored.
 */
#ifndef NYXJ_SCHEMA_H
#define NYXJ_SCHEMA_H

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NYXJ_SCHEMA_MAX_ISSUES 32

typedef struct {
    char path[128];    /* Location in the instance, e.g. "$.user.age" */
    char keyword[24];  /* Failing keyword, e.g. "minimum" */
    char message[160];
} nyxj_schema_issue;

typedef struct {
    int count;  /* Issues stored below */
    int total;  /* Issues found (may exceed NYXJ_SCHEMA_MAX_ISSUES) */
    nyxj_schema_issue issues[NYXJ_SCHEMA_MAX_ISSUES];
} nyxj_schema_report;

/* Returns true when `instance` matches `schema`. `report` may be NULL. */
bool nyxj_schema_validate(const nyxj_value* schema, const nyxj_value* instance, nyxj_schema_report* report);

/* Convenience: parse both documents then validate. */
bool nyxj_schema_validate_json(const char* schema_json, const char* instance_json, nyxj_schema_report* report,
                               nyxj_error* err);

/* [{"path":..., "keyword":..., "message":...}, ...] */
nyxj_value* nyxj_schema_report_to_value(const nyxj_schema_report* report);

#ifdef __cplusplus
}
#endif

#endif
