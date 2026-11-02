#include "../../include/nyxj.h"

#include <string.h>

/* Memory pool: values are carved out of large blocks and released at once. */

typedef struct arena_block {
    struct arena_block* next;
    size_t size;
    size_t used;
    unsigned char* data;
} arena_block;

struct nyxj_arena {
    arena_block* head;
    size_t block_size;
    size_t used;
};

static arena_block* block_new(size_t size) {
    arena_block* b = malloc(sizeof(arena_block) + size + 15);
    if (!b) return NULL;
    uintptr_t base = (uintptr_t)(b + 1);
    b->data = (unsigned char*)((base + 15) & ~(uintptr_t)15);
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

nyxj_arena* nyxj_arena_create(size_t block_size) {
    nyxj_arena* a = calloc(1, sizeof(*a));
    if (!a) return NULL;
    a->block_size = block_size ? block_size : 64 * 1024;
    return a;
}

void* nyxj_arena_alloc(nyxj_arena* a, size_t size) {
    if (!a) return NULL;
    size = (size + 15) & ~(size_t)15;
    if (size == 0) size = 16;
    if (!a->head || a->head->used + size > a->head->size) {
        size_t bsize = size > a->block_size ? size : a->block_size;
        arena_block* b = block_new(bsize);
        if (!b) return NULL;
        b->next = a->head;
        a->head = b;
    }
    void* p = a->head->data + a->head->used;
    a->head->used += size;
    a->used += size;
    return p;
}

char* nyxj_arena_strndup(nyxj_arena* a, const char* s, size_t len) {
    char* d = nyxj_arena_alloc(a, len + 1);
    if (!d) return NULL;
    if (len) memcpy(d, s, len);
    d[len] = '\0';
    return d;
}

void nyxj_arena_reset(nyxj_arena* a) {
    if (!a) return;
    /* Keep the most recent block for reuse, release the others. */
    arena_block* keep = a->head;
    if (keep) {
        arena_block* b = keep->next;
        while (b) { arena_block* n = b->next; free(b); b = n; }
        keep->next = NULL;
        keep->used = 0;
    }
    a->used = 0;
}

size_t nyxj_arena_used(const nyxj_arena* a) { return a ? a->used : 0; }

void nyxj_arena_destroy(nyxj_arena* a) {
    if (!a) return;
    arena_block* b = a->head;
    while (b) { arena_block* n = b->next; free(b); b = n; }
    free(a);
}
