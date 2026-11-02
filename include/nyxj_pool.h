/*
 * Fixed-size worker thread pool with a bounded queue.
 *
 * Huge JSON payloads (several MB) are parsed on workers so the network
 * event loop never blocks. A full queue rejects work instead of growing
 * without bound (backpressure), and the active-worker limit can be lowered
 * at runtime (Eco-Throttle).
 */
#ifndef NYXJ_POOL_H
#define NYXJ_POOL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*nyxj_task_fn)(void* arg);
typedef struct nyxj_pool nyxj_pool;

typedef struct {
    int threads;
    int queue_capacity;
    int queued;
    int active;
    int concurrency_limit;
    uint64_t completed;
    uint64_t rejected;
} nyxj_pool_stats;

nyxj_pool* nyxj_pool_create(int threads, int queue_capacity); /* threads <= 0: one per CPU */
bool       nyxj_pool_submit(nyxj_pool* pool, nyxj_task_fn fn, void* arg); /* false when full */
void       nyxj_pool_wait(nyxj_pool* pool);                            /* Until idle */
void       nyxj_pool_set_concurrency(nyxj_pool* pool, int max_active); /* 0 = all threads */
void       nyxj_pool_stats_get(nyxj_pool* pool, nyxj_pool_stats* out);
void       nyxj_pool_destroy(nyxj_pool* pool); /* Drains the queue, then joins */

#ifdef __cplusplus
}
#endif

#endif
