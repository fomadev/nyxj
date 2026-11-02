#include "../../include/nyxj.h"
#include "internal.h"

#include <string.h>

/* --- String helpers --- */

char* nyxj_strndup(const char* s, size_t len) {
    char* d = malloc(len + 1);
    if (!d) return NULL;
    if (len) memcpy(d, s, len); /* s may be NULL when len is 0 */
    d[len] = '\0';
    return d;
}

char* nyxj_strdup(const char* s) { return s ? nyxj_strndup(s, strlen(s)) : NULL; }

/* --- Builder API --- */

static nyxj_value* new_value(nyxj_type type) {
    nyxj_value* v = calloc(1, sizeof(nyxj_value));
    if (v) v->type = type;
    return v;
}

nyxj_value* nyxj_new_null(void) { return new_value(NYXJ_NULL); }

nyxj_value* nyxj_new_bool(bool b) {
    nyxj_value* v = new_value(NYXJ_BOOL);
    if (v) v->as.boolean = b;
    return v;
}

nyxj_value* nyxj_new_number(double n) {
    nyxj_value* v = new_value(NYXJ_NUMBER);
    if (v) v->as.number = n;
    return v;
}

nyxj_value* nyxj_new_stringn(const char* s, size_t len) {
    nyxj_value* v = new_value(NYXJ_STRING);
    if (!v) return NULL;
    v->as.string = nyxj_strndup(s ? s : "", s ? len : 0);
    if (!v->as.string) { free(v); return NULL; }
    return v;
}

nyxj_value* nyxj_new_string(const char* s) { return nyxj_new_stringn(s, s ? strlen(s) : 0); }
nyxj_value* nyxj_new_array(void) { return new_value(NYXJ_ARRAY); }
nyxj_value* nyxj_new_object(void) { return new_value(NYXJ_OBJECT); }

static bool array_grow(nyxj_value* arr) {
    if (arr->as.array.count < arr->as.array.capacity) return true;
    int ncap = arr->as.array.capacity ? arr->as.array.capacity * 2 : 4;
    nyxj_value** items = realloc(arr->as.array.items, sizeof(nyxj_value*) * (size_t)ncap);
    if (!items) return false;
    arr->as.array.items = items;
    arr->as.array.capacity = ncap;
    return true;
}

bool nyxj_array_push(nyxj_value* arr, nyxj_value* item) {
    if (!arr || !item || arr->type != NYXJ_ARRAY || (arr->flags & NYXJ_VFLAG_ARENA)) return false;
    if (!array_grow(arr)) return false;
    arr->as.array.items[arr->as.array.count++] = item;
    return true;
}

bool nyxj_array_insert(nyxj_value* arr, int index, nyxj_value* item) {
    if (!arr || !item || arr->type != NYXJ_ARRAY || (arr->flags & NYXJ_VFLAG_ARENA)) return false;
    if (index < 0 || index > arr->as.array.count) return false;
    if (!array_grow(arr)) return false;
    memmove(&arr->as.array.items[index + 1], &arr->as.array.items[index],
            sizeof(nyxj_value*) * (size_t)(arr->as.array.count - index));
    arr->as.array.items[index] = item;
    arr->as.array.count++;
    return true;
}

nyxj_value* nyxj_array_get(const nyxj_value* arr, int index) {
    if (!arr || arr->type != NYXJ_ARRAY) return NULL;
    if (index < 0) index += arr->as.array.count;
    if (index < 0 || index >= arr->as.array.count) return NULL;
    return arr->as.array.items[index];
}

nyxj_value* nyxj_array_detach(nyxj_value* arr, int index) {
    if (!arr || arr->type != NYXJ_ARRAY || (arr->flags & NYXJ_VFLAG_ARENA)) return NULL;
    if (index < 0 || index >= arr->as.array.count) return NULL;
    nyxj_value* item = arr->as.array.items[index];
    memmove(&arr->as.array.items[index], &arr->as.array.items[index + 1],
            sizeof(nyxj_value*) * (size_t)(arr->as.array.count - index - 1));
    arr->as.array.count--;
    return item;
}

bool nyxj_array_remove(nyxj_value* arr, int index) {
    nyxj_value* item = nyxj_array_detach(arr, index);
    if (!item) return false;
    nyxj_free_value(item);
    return true;
}

bool nyxj_object_set(nyxj_value* obj, const char* key, nyxj_value* val) {
    if (!obj || !key || !val || obj->type != NYXJ_OBJECT || (obj->flags & NYXJ_VFLAG_ARENA)) return false;
    nyxj_pair* last = NULL;
    for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next) {
        if (strcmp(p->key, key) == 0) {
            if (p->value != val) nyxj_free_value(p->value);
            p->value = val;
            return true;
        }
        last = p;
    }
    nyxj_pair* pair = malloc(sizeof(nyxj_pair));
    if (!pair) return false;
    pair->key = nyxj_strdup(key);
    if (!pair->key) { free(pair); return false; }
    pair->value = val;
    pair->next = NULL;
    if (last) last->next = pair;
    else obj->as.object.pairs = pair;
    obj->as.object.count++;
    return true;
}

nyxj_value* nyxj_object_detach(nyxj_value* obj, const char* key) {
    if (!obj || !key || obj->type != NYXJ_OBJECT || (obj->flags & NYXJ_VFLAG_ARENA)) return NULL;
    nyxj_pair* prev = NULL;
    for (nyxj_pair* p = obj->as.object.pairs; p; prev = p, p = p->next) {
        if (strcmp(p->key, key) == 0) {
            if (prev) prev->next = p->next;
            else obj->as.object.pairs = p->next;
            nyxj_value* v = p->value;
            free(p->key);
            free(p);
            obj->as.object.count--;
            return v;
        }
    }
    return NULL;
}

bool nyxj_object_remove(nyxj_value* obj, const char* key) {
    nyxj_value* v = nyxj_object_detach(obj, key);
    if (!v) return false;
    nyxj_free_value(v);
    return true;
}

static bool set_owned(nyxj_value* obj, const char* key, nyxj_value* v) {
    if (!v) return false;
    if (!nyxj_object_set(obj, key, v)) { nyxj_free_value(v); return false; }
    return true;
}

bool nyxj_object_set_string(nyxj_value* obj, const char* key, const char* s) { return set_owned(obj, key, nyxj_new_string(s)); }
bool nyxj_object_set_number(nyxj_value* obj, const char* key, double n) { return set_owned(obj, key, nyxj_new_number(n)); }
bool nyxj_object_set_bool(nyxj_value* obj, const char* key, bool b) { return set_owned(obj, key, nyxj_new_bool(b)); }
bool nyxj_object_set_null(nyxj_value* obj, const char* key) { return set_owned(obj, key, nyxj_new_null()); }

/* --- Deep copy & comparison --- */

nyxj_value* nyxj_clone(const nyxj_value* v) {
    if (!v) return NULL;
    switch (v->type) {
        case NYXJ_NULL: return nyxj_new_null();
        case NYXJ_BOOL: return nyxj_new_bool(v->as.boolean);
        case NYXJ_NUMBER: return nyxj_new_number(v->as.number);
        case NYXJ_STRING: return nyxj_new_string(v->as.string);
        case NYXJ_ARRAY: {
            nyxj_value* a = nyxj_new_array();
            if (!a) return NULL;
            int n = v->as.array.count;
            if (n > 0) {
                a->as.array.items = malloc(sizeof(nyxj_value*) * (size_t)nyxj_array_capacity_for(n));
                if (!a->as.array.items) { free(a); return NULL; }
                a->as.array.capacity = nyxj_array_capacity_for(n);
            }
            for (int i = 0; i < n; i++) {
                nyxj_value* c = nyxj_clone(v->as.array.items[i]);
                if (!c) { nyxj_free_value(a); return NULL; }
                a->as.array.items[a->as.array.count++] = c;
            }
            return a;
        }
        case NYXJ_OBJECT: {
            nyxj_value* o = nyxj_new_object();
            if (!o) return NULL;
            nyxj_pair* last = NULL;
            for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
                nyxj_pair* np = malloc(sizeof(nyxj_pair));
                if (!np) { nyxj_free_value(o); return NULL; }
                np->key = nyxj_strdup(p->key);
                np->value = nyxj_clone(p->value);
                np->next = NULL;
                if (!np->key || !np->value) {
                    free(np->key);
                    nyxj_free_value(np->value);
                    free(np);
                    nyxj_free_value(o);
                    return NULL;
                }
                if (last) last->next = np;
                else o->as.object.pairs = np;
                last = np;
                o->as.object.count++;
            }
            return o;
        }
    }
    return NULL;
}

bool nyxj_equals(const nyxj_value* a, const nyxj_value* b) {
    if (a == b) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case NYXJ_NULL: return true;
        case NYXJ_BOOL: return a->as.boolean == b->as.boolean;
        case NYXJ_NUMBER: return a->as.number == b->as.number;
        case NYXJ_STRING: return strcmp(a->as.string, b->as.string) == 0;
        case NYXJ_ARRAY:
            if (a->as.array.count != b->as.array.count) return false;
            for (int i = 0; i < a->as.array.count; i++)
                if (!nyxj_equals(a->as.array.items[i], b->as.array.items[i])) return false;
            return true;
        case NYXJ_OBJECT:
            if (a->as.object.count != b->as.object.count) return false;
            for (nyxj_pair* p = a->as.object.pairs; p; p = p->next) {
                nyxj_value* other = nyxj_get_member(b, p->key);
                if (!other || !nyxj_equals(p->value, other)) return false;
            }
            return true;
    }
    return false;
}

int nyxj_size(const nyxj_value* v) {
    if (!v) return 0;
    switch (v->type) {
        case NYXJ_ARRAY: return v->as.array.count;
        case NYXJ_OBJECT: return v->as.object.count;
        case NYXJ_STRING: return (int)strlen(v->as.string);
        default: return 0;
    }
}

const char* nyxj_type_name(nyxj_type type) {
    switch (type) {
        case NYXJ_NULL: return "null";
        case NYXJ_BOOL: return "boolean";
        case NYXJ_NUMBER: return "number";
        case NYXJ_STRING: return "string";
        case NYXJ_ARRAY: return "array";
        case NYXJ_OBJECT: return "object";
    }
    return "unknown";
}

/* --- Cleanup --- */

void nyxj_free_value(nyxj_value* v) {
    if (!v || (v->flags & NYXJ_VFLAG_ARENA)) return; /* Arena values are released with the arena */
    switch (v->type) {
        case NYXJ_STRING: free(v->as.string); break;
        case NYXJ_ARRAY:
            for (int i = 0; i < v->as.array.count; i++) nyxj_free_value(v->as.array.items[i]);
            free(v->as.array.items);
            break;
        case NYXJ_OBJECT: {
            nyxj_pair* p = v->as.object.pairs;
            while (p) {
                nyxj_pair* n = p->next;
                free(p->key);
                nyxj_free_value(p->value);
                free(p);
                p = n;
            }
            break;
        }
        default: break;
    }
    free(v);
}

void nyxj_result_free(nyxj_result* r) {
    if (!r) return;
    nyxj_free_value(r->root);
    r->root = NULL;
}

/* --- Getter API --- */

nyxj_value* nyxj_get_member(const nyxj_value* obj, const char* key) {
    if (!obj || !key || obj->type != NYXJ_OBJECT) return NULL;
    for (nyxj_pair* p = obj->as.object.pairs; p; p = p->next)
        if (strcmp(p->key, key) == 0) return p->value;
    return NULL;
}

const char* nyxj_get_string(const nyxj_value* obj, const char* key) {
    nyxj_value* m = nyxj_get_member(obj, key);
    return (m && m->type == NYXJ_STRING) ? m->as.string : NULL;
}

double nyxj_get_number(const nyxj_value* obj, const char* key) {
    nyxj_value* m = nyxj_get_member(obj, key);
    return (m && m->type == NYXJ_NUMBER) ? m->as.number : 0.0;
}

int64_t nyxj_get_int(const nyxj_value* obj, const char* key) {
    nyxj_value* m = nyxj_get_member(obj, key);
    if (!m || m->type != NYXJ_NUMBER) return 0;
    double d = m->as.number;
    if (d >= 9.2233720368547758e18) return INT64_MAX;
    if (d <= -9.2233720368547758e18) return INT64_MIN;
    return (int64_t)d;
}

bool nyxj_get_bool(const nyxj_value* obj, const char* key) {
    nyxj_value* m = nyxj_get_member(obj, key);
    return (m && m->type == NYXJ_BOOL) ? m->as.boolean : false;
}

/* --- Dot-path access --- */

nyxj_value* nyxj_get_path(const nyxj_value* root, const char* path) {
    if (!root || !path) return NULL;
    const nyxj_value* cur = root;
    const char* p = path;
    if (p[0] == '$') p++;
    if (p[0] == '.') p++;
    char key[256];
    while (*p && cur) {
        if (*p == '[') {
            p++;
            bool neg = false;
            if (*p == '-') { neg = true; p++; }
            if (*p < '0' || *p > '9') return NULL;
            long idx = 0;
            while (*p >= '0' && *p <= '9') {
                idx = idx * 10 + (*p++ - '0');
                if (idx > 0x7FFFFFFF) return NULL;
            }
            if (*p != ']') return NULL;
            p++;
            cur = nyxj_array_get(cur, neg ? (int)-idx : (int)idx);
        } else {
            size_t n = 0;
            while (*p && *p != '.' && *p != '[') {
                if (n + 1 >= sizeof(key)) return NULL;
                key[n++] = *p++;
            }
            key[n] = '\0';
            cur = nyxj_get_member(cur, key);
        }
        if (*p == '.') p++;
    }
    return (nyxj_value*)cur;
}

const char* nyxj_path_string(const nyxj_value* root, const char* path, const char* fallback) {
    nyxj_value* v = nyxj_get_path(root, path);
    return (v && v->type == NYXJ_STRING) ? v->as.string : fallback;
}

double nyxj_path_number(const nyxj_value* root, const char* path, double fallback) {
    nyxj_value* v = nyxj_get_path(root, path);
    return (v && v->type == NYXJ_NUMBER) ? v->as.number : fallback;
}

bool nyxj_path_bool(const nyxj_value* root, const char* path, bool fallback) {
    nyxj_value* v = nyxj_get_path(root, path);
    return (v && v->type == NYXJ_BOOL) ? v->as.boolean : fallback;
}
