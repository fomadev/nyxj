#include "../../include/nyxj_patch.h"
#include "../common/buf.h"

#include <stdio.h>
#include <string.h>

/* --- JSON Pointer --- */

/* Reads the next reference token of `p` into `out` (unescaping ~0 and ~1). */
static const char* next_token(const char* p, char* out, size_t n, bool* ok) {
    size_t k = 0;
    *ok = true;
    while (*p && *p != '/') {
        char ch = *p++;
        if (ch == '~') {
            if (*p == '0') ch = '~';
            else if (*p == '1') ch = '/';
            else { *ok = false; return p; }
            p++;
        }
        if (k + 1 >= n) { *ok = false; return p; }
        out[k++] = ch;
    }
    out[k] = '\0';
    return p;
}

static bool parse_index(const char* tok, int count, bool allow_end, int* out) {
    if (allow_end && strcmp(tok, "-") == 0) { *out = count; return true; }
    if (!*tok || (tok[0] == '0' && tok[1])) return false;
    long v = 0;
    for (const char* p = tok; *p; p++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (*p - '0');
        if (v > 0x7FFFFFFF) return false;
    }
    if (v > count || (!allow_end && v >= count)) return false;
    *out = (int)v;
    return true;
}

nyxj_value* nyxj_pointer_get(const nyxj_value* root, const char* pointer) {
    if (!root || !pointer) return NULL;
    const nyxj_value* cur = root;
    const char* p = pointer;
    char tok[512];
    while (*p) {
        if (*p != '/') return NULL;
        bool ok;
        p = next_token(p + 1, tok, sizeof(tok), &ok);
        if (!ok) return NULL;
        if (cur->type == NYXJ_OBJECT) {
            cur = nyxj_get_member(cur, tok);
        } else if (cur->type == NYXJ_ARRAY) {
            int idx;
            if (!parse_index(tok, cur->as.array.count, false, &idx)) return NULL;
            cur = cur->as.array.items[idx];
        } else {
            return NULL;
        }
        if (!cur) return NULL;
    }
    return (nyxj_value*)cur;
}

/* Splits "/a/b/c" into the parent pointer target and the last token. */
static nyxj_value* resolve_parent(nyxj_value* root, const char* path, char* last, size_t n) {
    const char* slash = strrchr(path, '/');
    if (!slash) return NULL;
    size_t plen = (size_t)(slash - path);
    char* parent_ptr = malloc(plen + 1);
    if (!parent_ptr) return NULL;
    memcpy(parent_ptr, path, plen);
    parent_ptr[plen] = '\0';
    nyxj_value* parent = nyxj_pointer_get(root, parent_ptr);
    free(parent_ptr);
    bool ok;
    next_token(slash + 1, last, n, &ok);
    return ok ? parent : NULL;
}

/* --- JSON Patch application --- */

static bool op_add(nyxj_value** doc, const char* path, nyxj_value* val, nyxj_error* err) {
    if (!*path) { nyxj_free_value(*doc); *doc = val; return true; }
    char last[512];
    nyxj_value* parent = resolve_parent(*doc, path, last, sizeof(last));
    if (!parent) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "add: parent of '%s' does not exist", path); nyxj_free_value(val); return false; }
    if (parent->type == NYXJ_OBJECT) {
        if (nyxj_object_set(parent, last, val)) return true;
    } else if (parent->type == NYXJ_ARRAY) {
        int idx;
        if (parse_index(last, parent->as.array.count, true, &idx) && nyxj_array_insert(parent, idx, val)) return true;
        nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "add: invalid array index in '%s'", path);
        nyxj_free_value(val);
        return false;
    }
    nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "add: cannot add into '%s'", path);
    nyxj_free_value(val);
    return false;
}

static nyxj_value* op_detach(nyxj_value* doc, const char* path, nyxj_error* err, const char* opname) {
    char last[512];
    nyxj_value* parent = *path ? resolve_parent(doc, path, last, sizeof(last)) : NULL;
    nyxj_value* out = NULL;
    if (parent && parent->type == NYXJ_OBJECT) {
        out = nyxj_object_detach(parent, last);
    } else if (parent && parent->type == NYXJ_ARRAY) {
        int idx;
        if (parse_index(last, parent->as.array.count, false, &idx)) out = nyxj_array_detach(parent, idx);
    }
    if (!out) nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "%s: path '%s' does not exist", opname, path);
    return out;
}

static bool apply_one(nyxj_value** doc, const nyxj_value* op, int index, nyxj_error* err) {
    const char* name = nyxj_get_string(op, "op");
    const char* path = nyxj_get_string(op, "path");
    if (!name || !path || (*path && *path != '/')) {
        nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "operation %d: missing or invalid 'op'/'path'", index);
        return false;
    }
    const nyxj_value* value = nyxj_get_member(op, "value");
    const char* from = nyxj_get_string(op, "from");

    if (strcmp(name, "add") == 0) {
        if (!value) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "operation %d: 'add' requires a value", index); return false; }
        return op_add(doc, path, nyxj_clone(value), err);
    }
    if (strcmp(name, "remove") == 0) {
        nyxj_value* v = op_detach(*doc, path, err, "remove");
        nyxj_free_value(v);
        return v != NULL;
    }
    if (strcmp(name, "replace") == 0) {
        if (!value) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "operation %d: 'replace' requires a value", index); return false; }
        if (!nyxj_pointer_get(*doc, path)) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "replace: path '%s' does not exist", path); return false; }
        if (*path) nyxj_free_value(op_detach(*doc, path, err, "replace"));
        return op_add(doc, path, nyxj_clone(value), err);
    }
    if (strcmp(name, "move") == 0 || strcmp(name, "copy") == 0) {
        bool move = name[0] == 'm';
        if (!from || (*from && *from != '/')) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "operation %d: '%s' requires 'from'", index, name); return false; }
        if (move && strcmp(from, path) == 0) return true;
        size_t flen = strlen(from);
        if (move && strncmp(path, from, flen) == 0 && path[flen] == '/') {
            nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "move: cannot move '%s' into its own child", from);
            return false;
        }
        nyxj_value* v;
        if (move) {
            v = *from ? op_detach(*doc, from, err, "move") : NULL;
        } else {
            const nyxj_value* src = nyxj_pointer_get(*doc, from);
            v = src ? nyxj_clone(src) : NULL;
            if (!v) nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "copy: path '%s' does not exist", from);
        }
        if (!v) return false;
        return op_add(doc, path, v, err);
    }
    if (strcmp(name, "test") == 0) {
        const nyxj_value* cur = nyxj_pointer_get(*doc, path);
        if (!value || !cur || !nyxj_equals(cur, value)) {
            nyxj_error_set(err, NYXJ_ERR_TEST_FAILED, "test: value at '%s' does not match", path);
            return false;
        }
        return true;
    }
    nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "operation %d: unknown op '%s'", index, name);
    return false;
}

bool nyxj_patch_apply(nyxj_value** doc, const nyxj_value* patch, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!doc || !*doc || !patch) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    if (patch->type != NYXJ_ARRAY) { nyxj_error_set(err, NYXJ_ERR_PATCH_FAILED, "patch must be an array of operations"); return false; }
    nyxj_value* work = nyxj_clone(*doc); /* Atomicity: mutate a copy, swap on success */
    if (!work) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return false; }
    for (int i = 0; i < patch->as.array.count; i++) {
        if (!apply_one(&work, patch->as.array.items[i], i, err)) {
            nyxj_free_value(work);
            return false;
        }
    }
    nyxj_free_value(*doc);
    *doc = work;
    return true;
}

/* --- Diff --- */

static void push_token(nyxj_buf* path, const char* tok) {
    nyxj_buf_putc(path, '/');
    for (const char* p = tok; *p; p++) {
        if (*p == '~') nyxj_buf_append(path, "~0", 2);
        else if (*p == '/') nyxj_buf_append(path, "~1", 2);
        else nyxj_buf_putc(path, *p);
    }
}

static void emit(nyxj_value* ops, const char* op, const nyxj_buf* path, const nyxj_value* value) {
    nyxj_value* o = nyxj_new_object();
    if (!o) return;
    nyxj_object_set_string(o, "op", op);
    nyxj_object_set_string(o, "path", path->data ? path->data : "");
    if (value) nyxj_object_set(o, "value", nyxj_clone(value));
    nyxj_array_push(ops, o);
}

static void diff_rec(nyxj_value* ops, nyxj_buf* path, const nyxj_value* a, const nyxj_value* b) {
    if (nyxj_equals(a, b)) return;
    size_t mark = path->len;
    if (a->type == NYXJ_OBJECT && b->type == NYXJ_OBJECT) {
        for (nyxj_pair* p = a->as.object.pairs; p; p = p->next) {
            if (!nyxj_get_member(b, p->key)) {
                push_token(path, p->key);
                emit(ops, "remove", path, NULL);
                path->len = mark;
                if (path->data) path->data[mark] = '\0';
            }
        }
        for (nyxj_pair* p = b->as.object.pairs; p; p = p->next) {
            const nyxj_value* av = nyxj_get_member(a, p->key);
            push_token(path, p->key);
            if (!av) emit(ops, "add", path, p->value);
            else diff_rec(ops, path, av, p->value);
            path->len = mark;
            if (path->data) path->data[mark] = '\0';
        }
        return;
    }
    if (a->type == NYXJ_ARRAY && b->type == NYXJ_ARRAY) {
        int na = a->as.array.count, nb = b->as.array.count;
        int common = na < nb ? na : nb;
        char idx[16];
        for (int i = 0; i < common; i++) {
            snprintf(idx, sizeof(idx), "%d", i);
            push_token(path, idx);
            diff_rec(ops, path, a->as.array.items[i], b->as.array.items[i]);
            path->len = mark;
            if (path->data) path->data[mark] = '\0';
        }
        for (int i = common; i < nb; i++) {
            snprintf(idx, sizeof(idx), "%d", i);
            push_token(path, idx);
            emit(ops, "add", path, b->as.array.items[i]);
            path->len = mark;
            if (path->data) path->data[mark] = '\0';
        }
        for (int i = na - 1; i >= common; i--) {
            snprintf(idx, sizeof(idx), "%d", i);
            push_token(path, idx);
            emit(ops, "remove", path, NULL);
            path->len = mark;
            if (path->data) path->data[mark] = '\0';
        }
        return;
    }
    emit(ops, "replace", path, b);
}

nyxj_value* nyxj_diff(const nyxj_value* from, const nyxj_value* to) {
    if (!from || !to) return NULL;
    nyxj_value* ops = nyxj_new_array();
    if (!ops) return NULL;
    nyxj_buf path;
    nyxj_buf_init(&path, 64);
    diff_rec(ops, &path, from, to);
    nyxj_buf_free(&path);
    return ops;
}

/* --- Merge Patch --- */

nyxj_value* nyxj_merge_patch(nyxj_value* target, const nyxj_value* patch) {
    if (!patch) return target;
    if (patch->type != NYXJ_OBJECT) {
        nyxj_free_value(target);
        return nyxj_clone(patch);
    }
    if (!target || target->type != NYXJ_OBJECT) {
        nyxj_free_value(target);
        target = nyxj_new_object();
        if (!target) return NULL;
    }
    for (nyxj_pair* p = patch->as.object.pairs; p; p = p->next) {
        if (p->value->type == NYXJ_NULL) {
            nyxj_object_remove(target, p->key);
        } else {
            nyxj_value* cur = nyxj_object_detach(target, p->key);
            nyxj_value* merged = nyxj_merge_patch(cur, p->value);
            if (merged) nyxj_object_set(target, p->key, merged);
        }
    }
    return target;
}

nyxj_value* nyxj_merge_diff(const nyxj_value* from, const nyxj_value* to) {
    if (!from || !to) return NULL;
    if (from->type != NYXJ_OBJECT || to->type != NYXJ_OBJECT) return nyxj_clone(to);
    nyxj_value* out = nyxj_new_object();
    if (!out) return NULL;
    for (nyxj_pair* p = from->as.object.pairs; p; p = p->next)
        if (!nyxj_get_member(to, p->key)) nyxj_object_set_null(out, p->key);
    for (nyxj_pair* p = to->as.object.pairs; p; p = p->next) {
        const nyxj_value* old = nyxj_get_member(from, p->key);
        if (!old) {
            nyxj_object_set(out, p->key, nyxj_clone(p->value));
        } else if (!nyxj_equals(old, p->value)) {
            if (old->type == NYXJ_OBJECT && p->value->type == NYXJ_OBJECT)
                nyxj_object_set(out, p->key, nyxj_merge_diff(old, p->value));
            else
                nyxj_object_set(out, p->key, nyxj_clone(p->value));
        }
    }
    return out;
}
