#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_schema.h"
#include "../../include/nyxj_patch.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_REF_DEPTH 64

typedef struct {
    const nyxj_value* root;
    nyxj_schema_report* rep; /* NULL = silent mode (stop at first failure) */
    int depth;
} vctx;

static bool validate(vctx* c, const nyxj_value* s, const nyxj_value* v, const char* path);

static void issue(vctx* c, const char* path, const char* kw, const char* fmt, ...) {
    if (!c->rep) return;
    c->rep->total++;
    if (c->rep->count >= NYXJ_SCHEMA_MAX_ISSUES) return;
    nyxj_schema_issue* is = &c->rep->issues[c->rep->count++];
    snprintf(is->path, sizeof(is->path), "%s", path);
    snprintf(is->keyword, sizeof(is->keyword), "%s", kw);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(is->message, sizeof(is->message), fmt, ap);
    va_end(ap);
}

static bool silent_valid(vctx* c, const nyxj_value* s, const nyxj_value* v, const char* path) {
    vctx sub = {c->root, NULL, c->depth};
    return validate(&sub, s, v, path);
}

static const nyxj_value* kw(const nyxj_value* s, const char* name) { return nyxj_get_member(s, name); }

static bool num_kw(const nyxj_value* s, const char* name, double* out) {
    const nyxj_value* v = kw(s, name);
    if (!v || v->type != NYXJ_NUMBER) return false;
    *out = v->as.number;
    return true;
}

static bool is_integer(const nyxj_value* v) {
    return v->type == NYXJ_NUMBER && isfinite(v->as.number) && floor(v->as.number) == v->as.number;
}

static bool type_matches(const char* t, const nyxj_value* v) {
    if (strcmp(t, "integer") == 0) return is_integer(v);
    return strcmp(t, nyxj_type_name(v->type)) == 0;
}

static size_t utf8_length(const char* s) {
    size_t n = 0;
    for (; *s; s++)
        if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}

static void child_key(char* out, size_t n, const char* path, const char* key) { snprintf(out, n, "%s.%s", path, key); }
static void child_idx(char* out, size_t n, const char* path, int i) { snprintf(out, n, "%s[%d]", path, i); }

/* --- format --- */

static bool all_digits(const char* s, int n) {
    for (int i = 0; i < n; i++)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

static bool format_ok(const char* fmt, const char* s) {
    size_t n = strlen(s);
    if (strcmp(fmt, "email") == 0) {
        const char* at = strchr(s, '@');
        return at && at != s && strchr(at + 1, '.') && at[1] != '.' && s[n - 1] != '.' && !strchr(at + 1, '@');
    }
    if (strcmp(fmt, "uuid") == 0) {
        if (n != 36) return false;
        for (size_t i = 0; i < n; i++) {
            if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
            else if (!isxdigit((unsigned char)s[i])) return false;
        }
        return true;
    }
    if (strcmp(fmt, "ipv4") == 0) {
        int parts = 0;
        const char* p = s;
        while (*p) {
            int digits = 0, val = 0;
            while (isdigit((unsigned char)*p)) { val = val * 10 + (*p - '0'); p++; digits++; }
            if (digits == 0 || digits > 3 || val > 255) return false;
            parts++;
            if (*p == '.') p++;
            else if (*p) return false;
        }
        return parts == 4 && s[n - 1] != '.';
    }
    bool date = n >= 10 && all_digits(s, 4) && s[4] == '-' && all_digits(s + 5, 2) && s[7] == '-' && all_digits(s + 8, 2);
    if (strcmp(fmt, "date") == 0) return date && n == 10;
    if (strcmp(fmt, "date-time") == 0)
        return date && n >= 20 && (s[10] == 'T' || s[10] == 't' || s[10] == ' ') && all_digits(s + 11, 2) &&
               s[13] == ':' && all_digits(s + 14, 2) && s[16] == ':' && all_digits(s + 17, 2);
    return true; /* Unknown formats are annotations only */
}

/* --- Validation --- */

static bool validate_object(vctx* c, const nyxj_value* s, const nyxj_value* v, const char* path) {
    bool ok = true;
    char sub[256];
    const nyxj_value* req = kw(s, "required");
    if (req && req->type == NYXJ_ARRAY) {
        for (int i = 0; i < req->as.array.count; i++) {
            const nyxj_value* k = req->as.array.items[i];
            if (k->type == NYXJ_STRING && !nyxj_get_member(v, k->as.string)) {
                issue(c, path, "required", "missing required property '%s'", k->as.string);
                ok = false;
                if (!c->rep) return false;
            }
        }
    }
    double lim;
    if (num_kw(s, "minProperties", &lim) && v->as.object.count < lim) {
        issue(c, path, "minProperties", "expected at least %g properties, got %d", lim, v->as.object.count);
        ok = false;
    }
    if (num_kw(s, "maxProperties", &lim) && v->as.object.count > lim) {
        issue(c, path, "maxProperties", "expected at most %g properties, got %d", lim, v->as.object.count);
        ok = false;
    }
    const nyxj_value* props = kw(s, "properties");
    const nyxj_value* addl = kw(s, "additionalProperties");
    for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
        child_key(sub, sizeof(sub), path, p->key);
        const nyxj_value* ps = props ? nyxj_get_member(props, p->key) : NULL;
        if (ps) {
            if (!validate(c, ps, p->value, sub)) { ok = false; if (!c->rep) return false; }
        } else if (addl) {
            if (addl->type == NYXJ_BOOL && !addl->as.boolean) {
                issue(c, sub, "additionalProperties", "property '%s' is not allowed", p->key);
                ok = false;
                if (!c->rep) return false;
            } else if (addl->type == NYXJ_OBJECT && !validate(c, addl, p->value, sub)) {
                ok = false;
                if (!c->rep) return false;
            }
        }
    }
    return ok;
}

static bool validate_array(vctx* c, const nyxj_value* s, const nyxj_value* v, const char* path) {
    bool ok = true;
    char sub[256];
    int n = v->as.array.count;
    double lim;
    if (num_kw(s, "minItems", &lim) && n < lim) { issue(c, path, "minItems", "expected at least %g items, got %d", lim, n); ok = false; }
    if (num_kw(s, "maxItems", &lim) && n > lim) { issue(c, path, "maxItems", "expected at most %g items, got %d", lim, n); ok = false; }
    const nyxj_value* uniq = kw(s, "uniqueItems");
    if (uniq && uniq->type == NYXJ_BOOL && uniq->as.boolean) {
        for (int i = 0; i < n && ok; i++)
            for (int j = i + 1; j < n; j++)
                if (nyxj_equals(v->as.array.items[i], v->as.array.items[j])) {
                    issue(c, path, "uniqueItems", "items %d and %d are equal", i, j);
                    ok = false;
                    break;
                }
    }
    int start = 0;
    const nyxj_value* prefix = kw(s, "prefixItems");
    const nyxj_value* items = kw(s, "items");
    if (!prefix && items && items->type == NYXJ_ARRAY) prefix = items, items = NULL; /* Draft 4-2019 tuple form */
    if (prefix && prefix->type == NYXJ_ARRAY) {
        for (int i = 0; i < n && i < prefix->as.array.count; i++) {
            child_idx(sub, sizeof(sub), path, i);
            if (!validate(c, prefix->as.array.items[i], v->as.array.items[i], sub)) { ok = false; if (!c->rep) return false; }
        }
        start = prefix->as.array.count;
    }
    if (items && (items->type == NYXJ_OBJECT || items->type == NYXJ_BOOL)) {
        for (int i = start; i < n; i++) {
            child_idx(sub, sizeof(sub), path, i);
            if (!validate(c, items, v->as.array.items[i], sub)) { ok = false; if (!c->rep) return false; }
        }
    }
    const nyxj_value* contains = kw(s, "contains");
    if (contains) {
        bool found = false;
        for (int i = 0; i < n && !found; i++) found = silent_valid(c, contains, v->as.array.items[i], path);
        if (!found) { issue(c, path, "contains", "no item matches the 'contains' schema"); ok = false; }
    }
    return ok;
}

static bool validate(vctx* c, const nyxj_value* s, const nyxj_value* v, const char* path) {
    if (!s) return true;
    if (s->type == NYXJ_BOOL) {
        if (!s->as.boolean) issue(c, path, "false", "no value is allowed here");
        return s->as.boolean;
    }
    if (s->type != NYXJ_OBJECT) return true;
    if (++c->depth > MAX_REF_DEPTH) {
        c->depth--;
        issue(c, path, "$ref", "schema recursion is too deep");
        return false;
    }
    bool ok = true;
#define FAIL_FAST() do { ok = false; if (!c->rep) goto out; } while (0)

    const nyxj_value* ref = kw(s, "$ref");
    if (ref && ref->type == NYXJ_STRING) {
        const char* r = ref->as.string;
        const nyxj_value* target = NULL;
        if (strcmp(r, "#") == 0) target = c->root;
        else if (r[0] == '#') target = nyxj_pointer_get(c->root, r + 1);
        if (!target) { issue(c, path, "$ref", "unresolvable reference '%s'", r); FAIL_FAST(); }
        else if (!validate(c, target, v, path)) FAIL_FAST();
    }

    const nyxj_value* t = kw(s, "type");
    if (t) {
        bool match = false;
        if (t->type == NYXJ_STRING) match = type_matches(t->as.string, v);
        else if (t->type == NYXJ_ARRAY)
            for (int i = 0; i < t->as.array.count && !match; i++)
                match = t->as.array.items[i]->type == NYXJ_STRING && type_matches(t->as.array.items[i]->as.string, v);
        else match = true;
        if (!match) {
            issue(c, path, "type", "expected %s, got %s", t->type == NYXJ_STRING ? t->as.string : "one of the listed types",
                  nyxj_type_name(v->type));
            ok = false;
            goto out; /* Other keywords are meaningless on the wrong type */
        }
    }

    const nyxj_value* en = kw(s, "enum");
    if (en && en->type == NYXJ_ARRAY) {
        bool found = false;
        for (int i = 0; i < en->as.array.count && !found; i++) found = nyxj_equals(en->as.array.items[i], v);
        if (!found) { issue(c, path, "enum", "value is not one of the allowed values"); FAIL_FAST(); }
    }
    const nyxj_value* cst = kw(s, "const");
    if (cst && !nyxj_equals(cst, v)) { issue(c, path, "const", "value does not match the constant"); FAIL_FAST(); }

    if (v->type == NYXJ_NUMBER) {
        double x = v->as.number, lim;
        const nyxj_value* exmin = kw(s, "exclusiveMinimum");
        const nyxj_value* exmax = kw(s, "exclusiveMaximum");
        bool exmin_bool = exmin && exmin->type == NYXJ_BOOL && exmin->as.boolean;
        bool exmax_bool = exmax && exmax->type == NYXJ_BOOL && exmax->as.boolean;
        if (num_kw(s, "minimum", &lim) && (exmin_bool ? x <= lim : x < lim)) {
            issue(c, path, "minimum", "%g is less than the minimum %g", x, lim);
            FAIL_FAST();
        }
        if (num_kw(s, "maximum", &lim) && (exmax_bool ? x >= lim : x > lim)) {
            issue(c, path, "maximum", "%g is greater than the maximum %g", x, lim);
            FAIL_FAST();
        }
        if (num_kw(s, "exclusiveMinimum", &lim) && x <= lim) { issue(c, path, "exclusiveMinimum", "%g must be greater than %g", x, lim); FAIL_FAST(); }
        if (num_kw(s, "exclusiveMaximum", &lim) && x >= lim) { issue(c, path, "exclusiveMaximum", "%g must be less than %g", x, lim); FAIL_FAST(); }
        if (num_kw(s, "multipleOf", &lim) && lim > 0) {
            double q = x / lim;
            if (fabs(q - nearbyint(q)) > 1e-9) { issue(c, path, "multipleOf", "%g is not a multiple of %g", x, lim); FAIL_FAST(); }
        }
    } else if (v->type == NYXJ_STRING) {
        double lim;
        size_t len = utf8_length(v->as.string);
        if (num_kw(s, "minLength", &lim) && (double)len < lim) { issue(c, path, "minLength", "string is shorter than %g characters", lim); FAIL_FAST(); }
        if (num_kw(s, "maxLength", &lim) && (double)len > lim) { issue(c, path, "maxLength", "string is longer than %g characters", lim); FAIL_FAST(); }
        const nyxj_value* fmt = kw(s, "format");
        if (fmt && fmt->type == NYXJ_STRING && !format_ok(fmt->as.string, v->as.string)) {
            issue(c, path, "format", "string is not a valid '%s'", fmt->as.string);
            FAIL_FAST();
        }
    } else if (v->type == NYXJ_OBJECT) {
        if (!validate_object(c, s, v, path)) FAIL_FAST();
    } else if (v->type == NYXJ_ARRAY) {
        if (!validate_array(c, s, v, path)) FAIL_FAST();
    }

    const nyxj_value* all = kw(s, "allOf");
    if (all && all->type == NYXJ_ARRAY)
        for (int i = 0; i < all->as.array.count; i++)
            if (!validate(c, all->as.array.items[i], v, path)) FAIL_FAST();

    const nyxj_value* any = kw(s, "anyOf");
    if (any && any->type == NYXJ_ARRAY) {
        bool found = false;
        for (int i = 0; i < any->as.array.count && !found; i++) found = silent_valid(c, any->as.array.items[i], v, path);
        if (!found) { issue(c, path, "anyOf", "value does not match any of the schemas"); FAIL_FAST(); }
    }
    const nyxj_value* one = kw(s, "oneOf");
    if (one && one->type == NYXJ_ARRAY) {
        int matches = 0;
        for (int i = 0; i < one->as.array.count; i++) matches += silent_valid(c, one->as.array.items[i], v, path);
        if (matches != 1) { issue(c, path, "oneOf", "value matches %d schemas instead of exactly one", matches); FAIL_FAST(); }
    }
    const nyxj_value* not_ = kw(s, "not");
    if (not_ && silent_valid(c, not_, v, path)) { issue(c, path, "not", "value must not match the schema"); FAIL_FAST(); }

    const nyxj_value* if_ = kw(s, "if");
    if (if_) {
        const nyxj_value* branch = silent_valid(c, if_, v, path) ? kw(s, "then") : kw(s, "else");
        if (branch && !validate(c, branch, v, path)) FAIL_FAST();
    }
#undef FAIL_FAST
out:
    c->depth--;
    return ok;
}

bool nyxj_schema_validate(const nyxj_value* schema, const nyxj_value* instance, nyxj_schema_report* report) {
    if (report) memset(report, 0, sizeof(*report));
    if (!schema || !instance) return false;
    vctx c = {schema, report, 0};
    return validate(&c, schema, instance, "$");
}

bool nyxj_schema_validate_json(const char* schema_json, const char* instance_json, nyxj_schema_report* report,
                               nyxj_error* err) {
    nyxj_error_clear(err);
    nyxj_result s = nyxj_parse(schema_json);
    if (!s.is_valid) { if (err) *err = s.error; return false; }
    nyxj_result i = nyxj_parse(instance_json);
    if (!i.is_valid) { if (err) *err = i.error; nyxj_free_value(s.root); return false; }
    bool ok = nyxj_schema_validate(s.root, i.root, report);
    if (!ok) nyxj_error_set(err, NYXJ_ERR_SCHEMA_VIOLATION, NULL);
    nyxj_free_value(s.root);
    nyxj_free_value(i.root);
    return ok;
}

nyxj_value* nyxj_schema_report_to_value(const nyxj_schema_report* report) {
    nyxj_value* arr = nyxj_new_array();
    if (!arr || !report) return arr;
    for (int i = 0; i < report->count; i++) {
        nyxj_value* o = nyxj_new_object();
        if (!o) break;
        nyxj_object_set_string(o, "path", report->issues[i].path);
        nyxj_object_set_string(o, "keyword", report->issues[i].keyword);
        nyxj_object_set_string(o, "message", report->issues[i].message);
        nyxj_array_push(arr, o);
    }
    return arr;
}
