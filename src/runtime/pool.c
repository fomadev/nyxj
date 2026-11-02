#include "../../include/nyxj_pool.h"
#include "../common/thread.h"

#include <stdlib.h>

typedef struct {
    nyxj_task_fn fn;
    void* arg;
} task;

struct nyxj_pool {
    nyxj_mutex lock;
    nyxj_cond has_work;
    nyxj_cond idle;
    nyxj_thread* threads;
    int nthreads;
    task* queue;
    int cap, head, count;
    int active;
    int limit;
    bool shutdown;
    uint64_t completed, rejected;
};

#if !defined(NYXJ_NO_THREADS)
static void* worker(void* arg) {
    nyxj_pool* p = arg;
    for (;;) {
        nyxj_mutex_lock(&p->lock);
        while (!p->shutdown && (p->count == 0 || p->active >= p->limit)) nyxj_cond_wait(&p->has_work, &p->lock);
        if (p->count == 0 && p->shutdown) {
            nyxj_mutex_unlock(&p->lock);
            return NULL;
        }
        if (p->active >= p->limit) { /* Shutdown requested while throttled: keep draining */
            nyxj_mutex_unlock(&p->lock);
            nyxj_sleep_ms(1);
            continue;
        }
        task t = p->queue[p->head];
        p->head = (p->head + 1) % p->cap;
        p->count--;
        p->active++;
        nyxj_mutex_unlock(&p->lock);

        t.fn(t.arg);

        nyxj_mutex_lock(&p->lock);
        p->active--;
        p->completed++;
        if (p->count > 0) nyxj_cond_signal(&p->has_work);
        if (p->count == 0 && p->active == 0) nyxj_cond_broadcast(&p->idle);
        nyxj_mutex_unlock(&p->lock);
    }
}
#endif

nyxj_pool* nyxj_pool_create(int threads, int queue_capacity) {
    nyxj_pool* p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    if (threads <= 0) threads = nyxj_cpu_count();
    if (queue_capacity <= 0) queue_capacity = 1024;
    p->cap = queue_capacity;
    p->queue = calloc((size_t)queue_capacity, sizeof(task));
    p->threads = calloc((size_t)threads, sizeof(nyxj_thread));
    if (!p->queue || !p->threads) { free(p->queue); free(p->threads); free(p); return NULL; }
    nyxj_mutex_init(&p->lock);
    nyxj_cond_init(&p->has_work);
    nyxj_cond_init(&p->idle);
    p->limit = threads;
#if !defined(NYXJ_NO_THREADS)
    for (int i = 0; i < threads; i++) {
        if (!nyxj_thread_create(&p->threads[i], worker, p)) break;
        p->nthreads++;
    }
    if (p->nthreads == 0) { nyxj_pool_destroy(p); return NULL; }
    p->limit = p->nthreads;
#else
    p->nthreads = 0;
#endif
    return p;
}

bool nyxj_pool_submit(nyxj_pool* p, nyxj_task_fn fn, void* arg) {
    if (!p || !fn) return false;
#if defined(NYXJ_NO_THREADS)
    fn(arg); /* No threads: run inline */
    p->completed++;
    return true;
#else
    nyxj_mutex_lock(&p->lock);
    if (p->shutdown || p->count == p->cap) {
        p->rejected++;
        nyxj_mutex_unlock(&p->lock);
        return false;
    }
    p->queue[(p->head + p->count) % p->cap] = (task){fn, arg};
    p->count++;
    nyxj_cond_signal(&p->has_work);
    nyxj_mutex_unlock(&p->lock);
    return true;
#endif
}

void nyxj_pool_wait(nyxj_pool* p) {
    if (!p) return;
    nyxj_mutex_lock(&p->lock);
    while (p->count > 0 || p->active > 0) nyxj_cond_wait(&p->idle, &p->lock);
    nyxj_mutex_unlock(&p->lock);
}

void nyxj_pool_set_concurrency(nyxj_pool* p, int max_active) {
    if (!p) return;
    nyxj_mutex_lock(&p->lock);
    p->limit = (max_active <= 0 || max_active > p->nthreads) ? p->nthreads : max_active;
    nyxj_cond_broadcast(&p->has_work);
    nyxj_mutex_unlock(&p->lock);
}

void nyxj_pool_stats_get(nyxj_pool* p, nyxj_pool_stats* out) {
    if (!p || !out) return;
    nyxj_mutex_lock(&p->lock);
    out->threads = p->nthreads;
    out->queue_capacity = p->cap;
    out->queued = p->count;
    out->active = p->active;
    out->concurrency_limit = p->limit;
    out->completed = p->completed;
    out->rejected = p->rejected;
    nyxj_mutex_unlock(&p->lock);
}

void nyxj_pool_destroy(nyxj_pool* p) {
    if (!p) return;
    nyxj_mutex_lock(&p->lock);
    p->shutdown = true;
    p->limit = p->nthreads; /* Lift throttling so the queue drains */
    nyxj_cond_broadcast(&p->has_work);
    nyxj_mutex_unlock(&p->lock);
    for (int i = 0; i < p->nthreads; i++) nyxj_thread_join(p->threads[i]);
    nyxj_cond_destroy(&p->has_work);
    nyxj_cond_destroy(&p->idle);
    nyxj_mutex_destroy(&p->lock);
    free(p->threads);
    free(p->queue);
    free(p);
}
