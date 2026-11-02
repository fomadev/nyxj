#include "../../include/nyxj.h"
#include "../common/buf.h"

#include <string.h>

/* Linear-time serializer with full string escaping and round-trip numbers. */

typedef struct {
    nyxj_buf* b;
    int indent;
    bool sort_keys;
    bool escape_unicode;
} sctx;

static void newline(sctx* s, int depth) {
    if (!s->indent) return;
    nyxj_buf_putc(s->b, '\n');
    int n = depth * s->indent;
    if (!nyxj_buf_reserve(s->b, (size_t)n)) return;
    memset(s->b->data + s->b->len, ' ', (size_t)n);
    s->b->len += (size_t)n;
    s->b->data[s->b->len] = '\0';
}

static int cmp_pairs(const void* a, const void* b) {
    const nyxj_pair* pa = *(const nyxj_pair* const*)a;
    const nyxj_pair* pb = *(const nyxj_pair* const*)b;
    return strcmp(pa->key, pb->key);
}

static void write_value(sctx* s, const nyxj_value* v, int depth);

static void write_member(sctx* s, const nyxj_pair* p, int depth) {
    newline(s, depth);
    nyxj_buf_json_string(s->b, p->key, s->escape_unicode);
    if (s->indent) nyxj_buf_append(s->b, ": ", 2);
    else nyxj_buf_putc(s->b, ':');
    write_value(s, p->value, depth);
}

static void write_value(sctx* s, const nyxj_value* v, int depth) {
    if (!v) { nyxj_buf_append(s->b, "null", 4); return; }
    switch (v->type) {
        case NYXJ_NULL: nyxj_buf_append(s->b, "null", 4); break;
        case NYXJ_BOOL:
            if (v->as.boolean) nyxj_buf_append(s->b, "true", 4);
            else nyxj_buf_append(s->b, "false", 5);
            break;
        case NYXJ_NUMBER: nyxj_buf_json_number(s->b, v->as.number); break;
        case NYXJ_STRING: nyxj_buf_json_string(s->b, v->as.string, s->escape_unicode); break;
        case NYXJ_ARRAY:
            if (v->as.array.count == 0) { nyxj_buf_append(s->b, "[]", 2); break; }
            nyxj_buf_putc(s->b, '[');
            for (int i = 0; i < v->as.array.count; i++) {
                if (i) nyxj_buf_putc(s->b, ',');
                newline(s, depth + 1);
                write_value(s, v->as.array.items[i], depth + 1);
            }
            newline(s, depth);
            nyxj_buf_putc(s->b, ']');
            break;
        case NYXJ_OBJECT: {
            if (v->as.object.count == 0 || !v->as.object.pairs) { nyxj_buf_append(s->b, "{}", 2); break; }
            nyxj_buf_putc(s->b, '{');
            const nyxj_pair** sorted = NULL;
            int n = 0;
            if (s->sort_keys) {
                for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) n++;
                sorted = malloc(sizeof(nyxj_pair*) * (size_t)n);
            }
            if (sorted) {
                int i = 0;
                for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) sorted[i++] = p;
                qsort(sorted, (size_t)n, sizeof(nyxj_pair*), cmp_pairs);
                for (i = 0; i < n; i++) {
                    if (i) nyxj_buf_putc(s->b, ',');
                    write_member(s, sorted[i], depth + 1);
                }
                free(sorted);
            } else {
                for (nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
                    if (p != v->as.object.pairs) nyxj_buf_putc(s->b, ',');
                    write_member(s, p, depth + 1);
                }
            }
            newline(s, depth);
            nyxj_buf_putc(s->b, '}');
            break;
        }
    }
}

char* nyxj_serialize_ex(const nyxj_value* v, const nyxj_serialize_options* opts, size_t* out_len) {
    nyxj_buf b;
    nyxj_buf_init(&b, 256);
    sctx s;
    s.b = &b;
    s.indent = opts && opts->indent > 0 ? opts->indent : 0;
    s.sort_keys = opts ? opts->sort_keys : false;
    s.escape_unicode = opts ? opts->escape_unicode : false;
    write_value(&s, v, 0);
    return nyxj_buf_detach(&b, out_len);
}

char* nyxj_serialize(const nyxj_value* v) { return nyxj_serialize_ex(v, NULL, NULL); }

char* nyxj_serialize_pretty(const nyxj_value* v, int indent) {
    nyxj_serialize_options o = {indent > 0 ? indent : 2, false, false};
    return nyxj_serialize_ex(v, &o, NULL);
}
