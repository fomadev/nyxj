/*
 * Observability: Prometheus-compatible metrics registry and process/system
 * resource monitoring (CPU time, resident memory, load, CPU temperature).
 */
#ifndef NYXJ_METRICS_H
#define NYXJ_METRICS_H

#include <stdint.h>

#include "nyxj.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nyxj_metrics nyxj_metrics;
typedef struct nyxj_metric nyxj_metric;
typedef void (*nyxj_metrics_collector)(nyxj_metrics* m, void* user);

nyxj_metrics* nyxj_metrics_create(void);
void          nyxj_metrics_destroy(nyxj_metrics* m);

/*
 * Get-or-create a series. `labels` is the Prometheus label body without
 * braces, e.g. method="GET",route="/api" (NULL or "" for none).
 */
nyxj_metric* nyxj_metrics_counter(nyxj_metrics* m, const char* name, const char* help, const char* labels);
nyxj_metric* nyxj_metrics_gauge(nyxj_metrics* m, const char* name, const char* help, const char* labels);
nyxj_metric* nyxj_metrics_histogram(nyxj_metrics* m, const char* name, const char* help, const char* labels,
                                    const double* buckets, int nbuckets); /* NULL: latency buckets */

void   nyxj_metric_inc(nyxj_metric* metric);
void   nyxj_metric_add(nyxj_metric* metric, double delta);
void   nyxj_metric_set(nyxj_metric* metric, double value);
void   nyxj_metric_observe(nyxj_metric* metric, double value);
double nyxj_metric_value(nyxj_metric* metric); /* Counter/gauge value, histogram count */

/* Collectors run right before rendering (refresh gauges lazily). */
void  nyxj_metrics_add_collector(nyxj_metrics* m, nyxj_metrics_collector fn, void* user);
char* nyxj_metrics_render(nyxj_metrics* m); /* Text exposition format 0.0.4 */

/* Escapes a label value (backslash, quote, newline) into out. */
void nyxj_metrics_escape_label(const char* value, char* out, size_t out_size);

/* --- Resource monitoring --- */

typedef struct {
    double cpu_seconds;   /* User + system CPU time of this process */
    uint64_t rss_bytes;   /* Resident memory */
    double load1;         /* 1-minute load average, -1 when unavailable */
    double cpu_temp_c;    /* Hottest thermal zone in Celsius, -1 when unavailable */
    int cpu_count;
} nyxj_sys_stats;

bool nyxj_sys_stats_get(nyxj_sys_stats* out);

#ifdef __cplusplus
}
#endif

#endif
