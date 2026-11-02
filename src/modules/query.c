#include "../../include/nyxj_query.h"
#include "../core/internal.h"

#include <ctype.h>
#include <string.h>

/* --- Compiled representation --- */

typedef enum { SEG_CHILD, SEG_INDEX, SEG_SLICE, SEG_WILDCARD, SEG_FILTER, SEG_UNION } seg_kind;

typedef struct {
    bool is_index;
    int index;
    char* name;
} sel_item;

/* Simple relative/absolute path used inside filter operands (@.a.b[0]). */
typedef struct {
    int count;
    sel_item* items;
} simple_path;

typedef enum { OPD_LITERAL, OPD_CURRENT, OPD_ROOT } operand_kind;

typedef struct {
    operand_kind kind;
    nyxj_value* literal;
    simple_path path;
} operand;

typedef enum { F_OR, F_AND, F_NOT, F_CMP, F_EXISTS } fkind;
typedef enum { OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE } fop;

typedef struct fnode {
    fkind kind;
    struct fnode* a;
    struct fnode* b;
    fop op;
    operand lhs, rhs;
} fnode;

typedef struct {
    seg_kind kind;
    bool recursive;
    char* name;
    int index;
    int start, end, step;
    bool has_start, has_end;
    fnode* filter;
    int nitems;
    sel_item* items;
} qseg;

struct nyxj_compiled_query {
    int count;
    qseg* segs;
};

/* --- Parser --- */

typedef struct {
    const char* src;
    const char* p;
    nyxj_error* err;
    bool failed;
} qparser;

static bool qfail(qparser* q, const char* what) {
    if (!q->failed) {
        q->failed = true;
        nyxj_error_set(q->err, NYXJ_ERR_INVALID_QUERY, "%s at position %d", what, (int)(q->p - q->src));
        if (q->err) q->err->offset = (size_t)(q->p - q->src);
    }
    return false;
}

static void skip_sp(qparser* q) { while (*q->p == ' ' || *q->p == '\t') q->p++; }

static bool is_name_char(char c) { return isalnum((unsigned char)c) || c == '_' || c == '-' || c == '$' || (unsigned char)c >= 0x80; }

static char* parse_name(qparser* q) {
    const char* s = q->p;
    while (is_name_char(*q->p)) q->p++;
    if (q->p == s) { qfail(q, "expected a member name"); return NULL; }
    return nyxj_strndup(s, (size_t)(q->p - s));
}

static char* parse_quoted(qparser* q) {
    char quote = *q->p++;
    size_t cap = 32, n = 0;
    char* out = malloc(cap);
    if (!out) return NULL;
    while (*q->p && *q->p != quote) {
        char ch = *q->p++;
        if (ch == '\\' && *q->p) {
            ch = *q->p++;
            if (ch == 'n') ch = '\n';
            else if (ch == 't') ch = '\t';
        }
        if (n + 2 > cap) {
            char* g = realloc(out, cap *= 2);
            if (!g) { free(out); return NULL; }
            out = g;
        }
        out[n++] = ch;
    }
    if (*q->p != quote) { free(out); qfail(q, "unterminated string"); return NULL; }
    q->p++;
    out[n] = '\0';
    return out;
}

static bool parse_int(qparser* q, int* out) {
    const char* s = q->p;
    bool neg = false;
    if (*q->p == '-') { neg = true; q->p++; }
    if (!isdigit((unsigned char)*q->p)) { q->p = s; return false; }
    long v = 0;
    while (isdigit((unsigned char)*q->p)) {
        v = v * 10 + (*q->p++ - '0');
        if (v > 0x7FFFFFFF) v = 0x7FFFFFFF;
    }
    *out = (int)(neg ? -v : v);
    return true;
}

static void free_simple(simple_path* sp) {
    for (int i = 0; i < sp->count; i++) free(sp->items[i].name);
    free(sp->items);
    sp->items = NULL;
    sp->count = 0;
}

static bool push_item(sel_item** items, int* count, sel_item it) {
    sel_item* g = realloc(*items, sizeof(sel_item) * (size_t)(*count + 1));
    if (!g) { free(it.name); return false; }
    *items = g;
    (*items)[(*count)++] = it;
    return true;
}

static bool parse_simple_path(qparser* q, simple_path* sp) {
    for (;;) {
        if (*q->p == '.' && q->p[1] != '.') {
            q->p++;
            sel_item it = {false, 0, parse_name(q)};
            if (!it.name || !push_item(&sp->items, &sp->count, it)) return false;
        } else if (*q->p == '[') {
            q->p++;
            skip_sp(q);
            sel_item it = {false, 0, NULL};
            if (*q->p == '\'' || *q->p == '"') {
                it.name = parse_quoted(q);
                if (!it.name) return false;
            } else if (parse_int(q, &it.index)) {
                it.is_index = true;
            } else {
                return qfail(q, "expected an index or a quoted name");
            }
            skip_sp(q);
            if (*q->p != ']') { free(it.name); return qfail(q, "expected ']'"); }
            q->p++;
            if (!push_item(&sp->items, &sp->count, it)) return false;
        } else {
            return true;
        }
    }
}

static bool parse_operand(qparser* q, operand* o) {
    memset(o, 0, sizeof(*o));
    skip_sp(q);
    if (*q->p == '@' || *q->p == '$') {
        o->kind = *q->p == '@' ? OPD_CURRENT : OPD_ROOT;
        q->p++;
        return parse_simple_path(q, &o->path);
    }
    o->kind = OPD_LITERAL;
    if (*q->p == '\'' || *q->p == '"') {
        char* s = parse_quoted(q);
        if (!s) return false;
        o->literal = nyxj_new_string(s);
        free(s);
        return o->literal != NULL;
    }
    if (strncmp(q->p, "true", 4) == 0) { q->p += 4; o->literal = nyxj_new_bool(true); return true; }
    if (strncmp(q->p, "false", 5) == 0) { q->p += 5; o->literal = nyxj_new_bool(false); return true; }
    if (strncmp(q->p, "null", 4) == 0) { q->p += 4; o->literal = nyxj_new_null(); return true; }
    if (*q->p == '-' || isdigit((unsigned char)*q->p)) {
        char* end;
        double d = strtod(q->p, &end);
        if (end == q->p) return qfail(q, "invalid number");
        q->p = end;
        o->literal = nyxj_new_number(d);
        return true;
    }
    return qfail(q, "expected an operand");
}

static void free_filter(fnode* f) {
    if (!f) return;
    free_filter(f->a);
    free_filter(f->b);
    nyxj_free_value(f->lhs.literal);
    nyxj_free_value(f->rhs.literal);
    free_simple(&f->lhs.path);
    free_simple(&f->rhs.path);
    free(f);
}

static fnode* parse_or(qparser* q);

static fnode* new_fnode(fkind k) {
    fnode* f = calloc(1, sizeof(fnode));
    if (f) f->kind = k;
    return f;
}

static fnode* parse_unary(qparser* q) {
    skip_sp(q);
    if (*q->p == '!') {
        q->p++;
        fnode* inner = parse_unary(q);
        if (!inner) return NULL;
        fnode* f = new_fnode(F_NOT);
        if (!f) { free_filter(inner); return NULL; }
        f->a = inner;
        return f;
    }
    if (*q->p == '(') {
        q->p++;
        fnode* inner = parse_or(q);
        if (!inner) return NULL;
        skip_sp(q);
        if (*q->p != ')') { free_filter(inner); qfail(q, "expected ')'"); return NULL; }
        q->p++;
        return inner;
    }
    fnode* f = new_fnode(F_EXISTS);
    if (!f) return NULL;
    if (!parse_operand(q, &f->lhs)) { free_filter(f); return NULL; }
    skip_sp(q);
    static const struct { const char* tok; fop op; } ops[] = {
        {"==", OP_EQ}, {"!=", OP_NE}, {"<=", OP_LE}, {">=", OP_GE}, {"<", OP_LT}, {">", OP_GT}};
    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        size_t n = strlen(ops[i].tok);
        if (strncmp(q->p, ops[i].tok, n) == 0) {
            q->p += n;
            f->kind = F_CMP;
            f->op = ops[i].op;
            if (!parse_operand(q, &f->rhs)) { free_filter(f); return NULL; }
            return f;
        }
    }
    if (f->lhs.kind == OPD_LITERAL) { free_filter(f); qfail(q, "a literal alone is not a filter"); return NULL; }
    return f;
}

static fnode* parse_and(qparser* q) {
    fnode* left = parse_unary(q);
    while (left) {
        skip_sp(q);
        if (strncmp(q->p, "&&", 2) != 0) break;
        q->p += 2;
        fnode* right = parse_unary(q);
        fnode* f = right ? new_fnode(F_AND) : NULL;
        if (!f) { free_filter(left); free_filter(right); return NULL; }
        f->a = left;
        f->b = right;
        left = f;
    }
    return left;
}

static fnode* parse_or(qparser* q) {
    fnode* left = parse_and(q);
    while (left) {
        skip_sp(q);
        if (strncmp(q->p, "||", 2) != 0) break;
        q->p += 2;
        fnode* right = parse_and(q);
        fnode* f = right ? new_fnode(F_OR) : NULL;
        if (!f) { free_filter(left); free_filter(right); return NULL; }
        f->a = left;
        f->b = right;
        left = f;
    }
    return left;
}

static bool parse_bracket(qparser* q, qseg* s) {
    q->p++; /* '[' */
    skip_sp(q);
    if (*q->p == '*') {
        q->p++;
        s->kind = SEG_WILDCARD;
    } else if (*q->p == '?') {
        q->p++;
        skip_sp(q);
        bool paren = *q->p == '(';
        if (paren) q->p++;
        s->kind = SEG_FILTER;
        s->filter = parse_or(q);
        if (!s->filter) return false;
        skip_sp(q);
        if (paren) {
            if (*q->p != ')') return qfail(q, "expected ')'");
            q->p++;
        }
    } else if (*q->p == '\'' || *q->p == '"' || *q->p == '-' || isdigit((unsigned char)*q->p) || *q->p == ':') {
        /* Index, slice, name, or a union of them. */
        int a = 0;
        bool has_a = false;
        if (*q->p != '\'' && *q->p != '"') has_a = parse_int(q, &a);
        skip_sp(q);
        if (*q->p == ':') {
            s->kind = SEG_SLICE;
            s->has_start = has_a;
            s->start = a;
            s->step = 1;
            q->p++;
            skip_sp(q);
            s->has_end = parse_int(q, &s->end);
            skip_sp(q);
            if (*q->p == ':') {
                q->p++;
                skip_sp(q);
                if (!parse_int(q, &s->step)) s->step = 1;
            }
        } else {
            s->kind = SEG_UNION;
            for (;;) {
                sel_item it = {false, 0, NULL};
                if (has_a) {
                    it.is_index = true;
                    it.index = a;
                } else if (*q->p == '\'' || *q->p == '"') {
                    it.name = parse_quoted(q);
                    if (!it.name) return false;
                } else if (parse_int(q, &it.index)) {
                    it.is_index = true;
                } else {
                    return qfail(q, "expected an index or a quoted name");
                }
                has_a = false;
                if (!push_item(&s->items, &s->nitems, it)) return false;
                skip_sp(q);
                if (*q->p != ',') break;
                q->p++;
                skip_sp(q);
            }
            if (s->nitems == 1 && s->items[0].is_index) {
                s->kind = SEG_INDEX;
                s->index = s->items[0].index;
            } else if (s->nitems == 1) {
                s->kind = SEG_CHILD;
                s->name = s->items[0].name;
                s->items[0].name = NULL;
            }
        }
    } else {
        return qfail(q, "invalid bracket expression");
    }
    skip_sp(q);
    if (*q->p != ']') return qfail(q, "expected ']'");
    q->p++;
    return true;
}

static void free_seg(qseg* s) {
    free(s->name);
    free_filter(s->filter);
    for (int i = 0; i < s->nitems; i++) free(s->items[i].name);
    free(s->items);
}

void nyxj_query_free(nyxj_compiled_query* q) {
    if (!q) return;
    for (int i = 0; i < q->count; i++) free_seg(&q->segs[i]);
    free(q->segs);
    free(q);
}

nyxj_compiled_query* nyxj_query_compile(const char* path, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return NULL; }
    qparser q = {path, path, err, false};
    nyxj_compiled_query* cq = calloc(1, sizeof(*cq));
    if (!cq) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    skip_sp(&q);
    if (*q.p == '$') q.p++;
    else if (*q.p != '.' && *q.p != '[') { qfail(&q, "query must start with '$'"); nyxj_query_free(cq); return NULL; }
    while (*q.p && !q.failed) {
        qseg s;
        memset(&s, 0, sizeof(s));
        if (q.p[0] == '.' && q.p[1] == '.') {
            s.recursive = true;
            q.p += 2;
            if (*q.p == '*') { q.p++; s.kind = SEG_WILDCARD; }
            else if (*q.p == '[') parse_bracket(&q, &s);
            else { s.kind = SEG_CHILD; s.name = parse_name(&q); }
        } else if (*q.p == '.') {
            q.p++;
            if (*q.p == '*') { q.p++; s.kind = SEG_WILDCARD; }
            else { s.kind = SEG_CHILD; s.name = parse_name(&q); }
        } else if (*q.p == '[') {
            parse_bracket(&q, &s);
        } else if (*q.p == ' ') {
            skip_sp(&q);
            continue;
        } else {
            qfail(&q, "unexpected character");
        }
        qseg* g = q.failed ? NULL : realloc(cq->segs, sizeof(qseg) * (size_t)(cq->count + 1));
        if (!g) {
            free_seg(&s);
            if (!q.failed) qfail(&q, "out of memory");
            break;
        }
        cq->segs = g;
        cq->segs[cq->count++] = s;
    }
    if (q.failed) { nyxj_query_free(cq); return NULL; }
    return cq;
}

/* --- Evaluation --- */

typedef struct {
    const nyxj_value** items;
    size_t count, cap;
} nodelist;

static bool nl_push(nodelist* l, const nyxj_value* v) {
    if (l->count == l->cap) {
        size_t ncap = l->cap ? l->cap * 2 : 16;
        const nyxj_value** g = realloc((void*)l->items, sizeof(*g) * ncap);
        if (!g) return false;
        l->items = g;
        l->cap = ncap;
    }
    l->items[l->count++] = v;
    return true;
}

static const nyxj_value* resolve_simple(const nyxj_value* base, const simple_path* sp) {
    const nyxj_value* cur = base;
    for (int i = 0; i < sp->count && cur; i++)
        cur = sp->items[i].is_index ? nyxj_array_get(cur, sp->items[i].index) : nyxj_get_member(cur, sp->items[i].name);
    return cur;
}

static const nyxj_value* operand_value(const operand* o, const nyxj_value* cur, const nyxj_value* root) {
    if (o->kind == OPD_LITERAL) return o->literal;
    return resolve_simple(o->kind == OPD_CURRENT ? cur : root, &o->path);
}

static bool compare(fop op, const nyxj_value* a, const nyxj_value* b) {
    if (!a || !b) return op == OP_NE ? (a != b) : false;
    if (a->type == NYXJ_NUMBER && b->type == NYXJ_NUMBER) {
        double x = a->as.number, y = b->as.number;
        switch (op) {
            case OP_EQ: return x == y;
            case OP_NE: return x != y;
            case OP_LT: return x < y;
            case OP_LE: return x <= y;
            case OP_GT: return x > y;
            case OP_GE: return x >= y;
        }
    }
    if (a->type == NYXJ_STRING && b->type == NYXJ_STRING) {
        int c = strcmp(a->as.string, b->as.string);
        switch (op) {
            case OP_EQ: return c == 0;
            case OP_NE: return c != 0;
            case OP_LT: return c < 0;
            case OP_LE: return c <= 0;
            case OP_GT: return c > 0;
            case OP_GE: return c >= 0;
        }
    }
    if (op == OP_EQ) return nyxj_equals(a, b);
    if (op == OP_NE) return !nyxj_equals(a, b);
    return false;
}

static bool eval_filter(const fnode* f, const nyxj_value* cur, const nyxj_value* root) {
    switch (f->kind) {
        case F_OR: return eval_filter(f->a, cur, root) || eval_filter(f->b, cur, root);
        case F_AND: return eval_filter(f->a, cur, root) && eval_filter(f->b, cur, root);
        case F_NOT: return !eval_filter(f->a, cur, root);
        case F_EXISTS: {
            const nyxj_value* v = operand_value(&f->lhs, cur, root);
            return v != NULL && !(v->type == NYXJ_BOOL && !v->as.boolean);
        }
        case F_CMP: return compare(f->op, operand_value(&f->lhs, cur, root), operand_value(&f->rhs, cur, root));
    }
    return false;
}

static bool apply_seg(const qseg* s, const nyxj_value* node, const nyxj_value* root, nodelist* out) {
    switch (s->kind) {
        case SEG_CHILD: {
            const nyxj_value* m = nyxj_get_member(node, s->name);
            return m ? nl_push(out, m) : true;
        }
        case SEG_INDEX: {
            const nyxj_value* m = nyxj_array_get(node, s->index);
            return m ? nl_push(out, m) : true;
        }
        case SEG_WILDCARD:
            if (node->type == NYXJ_ARRAY) {
                for (int i = 0; i < node->as.array.count; i++)
                    if (!nl_push(out, node->as.array.items[i])) return false;
            } else if (node->type == NYXJ_OBJECT) {
                for (nyxj_pair* p = node->as.object.pairs; p; p = p->next)
                    if (!nl_push(out, p->value)) return false;
            }
            return true;
        case SEG_SLICE: {
            if (node->type != NYXJ_ARRAY || s->step == 0) return true;
            int n = node->as.array.count;
            int step = s->step;
            int start = s->has_start ? s->start : (step > 0 ? 0 : n - 1);
            int end = s->has_end ? s->end : (step > 0 ? n : -n - 1);
            if (start < 0) start += n;
            if (end < 0) end += n;
            if (step > 0) {
                if (start < 0) start = 0;
                if (end > n) end = n;
                for (int i = start; i < end; i += step)
                    if (!nl_push(out, node->as.array.items[i])) return false;
            } else {
                if (start >= n) start = n - 1;
                if (end < -1) end = -1;
                for (int i = start; i > end; i += step)
                    if (i >= 0 && !nl_push(out, node->as.array.items[i])) return false;
            }
            return true;
        }
        case SEG_FILTER:
            if (node->type == NYXJ_ARRAY) {
                for (int i = 0; i < node->as.array.count; i++)
                    if (eval_filter(s->filter, node->as.array.items[i], root) && !nl_push(out, node->as.array.items[i]))
                        return false;
            } else if (node->type == NYXJ_OBJECT) {
                for (nyxj_pair* p = node->as.object.pairs; p; p = p->next)
                    if (eval_filter(s->filter, p->value, root) && !nl_push(out, p->value)) return false;
            }
            return true;
        case SEG_UNION:
            for (int i = 0; i < s->nitems; i++) {
                const nyxj_value* m = s->items[i].is_index ? nyxj_array_get(node, s->items[i].index)
                                                           : nyxj_get_member(node, s->items[i].name);
                if (m && !nl_push(out, m)) return false;
            }
            return true;
    }
    return true;
}

static bool apply_recursive(const qseg* s, const nyxj_value* node, const nyxj_value* root, nodelist* out) {
    if (!apply_seg(s, node, root, out)) return false;
    if (node->type == NYXJ_ARRAY) {
        for (int i = 0; i < node->as.array.count; i++)
            if (!apply_recursive(s, node->as.array.items[i], root, out)) return false;
    } else if (node->type == NYXJ_OBJECT) {
        for (nyxj_pair* p = node->as.object.pairs; p; p = p->next)
            if (!apply_recursive(s, p->value, root, out)) return false;
    }
    return true;
}

int nyxj_query_exec(const nyxj_compiled_query* q, const nyxj_value* root, nyxj_query_fn fn, void* user) {
    if (!q || !root) return -1;
    nodelist cur = {0}, next = {0};
    if (!nl_push(&cur, root)) return -1;
    for (int i = 0; i < q->count; i++) {
        next.count = 0;
        for (size_t k = 0; k < cur.count; k++) {
            bool ok = q->segs[i].recursive ? apply_recursive(&q->segs[i], cur.items[k], root, &next)
                                           : apply_seg(&q->segs[i], cur.items[k], root, &next);
            if (!ok) { free((void*)cur.items); free((void*)next.items); return -1; }
        }
        nodelist t = cur;
        cur = next;
        next = t;
    }
    int n = 0;
    for (size_t k = 0; k < cur.count; k++) {
        n++;
        if (fn && !fn(cur.items[k], user)) break;
    }
    free((void*)cur.items);
    free((void*)next.items);
    return n;
}

/* --- One-shot helpers --- */

static bool collect_clone(const nyxj_value* m, void* user) {
    nyxj_value* c = nyxj_clone(m);
    if (c && !nyxj_array_push((nyxj_value*)user, c)) nyxj_free_value(c);
    return true;
}

static bool take_first(const nyxj_value* m, void* user) {
    *(const nyxj_value**)user = m;
    return false;
}

nyxj_value* nyxj_query(const nyxj_value* root, const char* path, nyxj_error* err) {
    nyxj_compiled_query* q = nyxj_query_compile(path, err);
    if (!q) return NULL;
    nyxj_value* out = nyxj_new_array();
    if (out && nyxj_query_exec(q, root, collect_clone, out) < 0) {
        nyxj_free_value(out);
        out = NULL;
        nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL);
    }
    nyxj_query_free(q);
    return out;
}

const nyxj_value* nyxj_query_first(const nyxj_value* root, const char* path, nyxj_error* err) {
    nyxj_compiled_query* q = nyxj_query_compile(path, err);
    if (!q) return NULL;
    const nyxj_value* first = NULL;
    nyxj_query_exec(q, root, take_first, &first);
    nyxj_query_free(q);
    if (!first) nyxj_error_set(err, NYXJ_ERR_NOT_FOUND, "no match for '%s'", path);
    return first;
}

int nyxj_query_count(const nyxj_value* root, const char* path, nyxj_error* err) {
    nyxj_compiled_query* q = nyxj_query_compile(path, err);
    if (!q) return -1;
    int n = nyxj_query_exec(q, root, NULL, NULL);
    nyxj_query_free(q);
    return n;
}
