/*
 * Streaming example: an endless IoT sensor feed (NDJSON) processed with the
 * SAX parser in small chunks. Memory stays constant whatever the volume.
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_sax.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char last_key[32];
    double sum;
    double max;
    unsigned long readings;
} sensor_stats;

static bool on_key(void* user, const char* key, size_t len) {
    sensor_stats* s = user;
    size_t n = len < sizeof(s->last_key) - 1 ? len : sizeof(s->last_key) - 1;
    memcpy(s->last_key, key, n);
    s->last_key[n] = '\0';
    return true;
}

static bool on_number(void* user, double value, const char* raw, size_t len) {
    (void)raw;
    (void)len;
    sensor_stats* s = user;
    if (strcmp(s->last_key, "celsius") == 0) {
        s->sum += value;
        if (s->readings == 0 || value > s->max) s->max = value;
        s->readings++;
    }
    return true;
}

int main(void) {
    sensor_stats stats;
    memset(&stats, 0, sizeof(stats));
    nyxj_sax_handler h;
    memset(&h, 0, sizeof(h));
    h.on_key = on_key;
    h.on_number = on_number;
    nyxj_sax_options opts;
    memset(&opts, 0, sizeof(opts));
    opts.multi_document = true; /* One JSON document per line */
    opts.max_token_size = 256;
    nyxj_sax* sax = nyxj_sax_create(&h, &stats, &opts);

    /* Simulate 200 000 readings arriving from the network, 4 KiB at a time. */
    char chunk[4096];
    size_t used = 0;
    unsigned long total_bytes = 0;
    for (int i = 0; i < 200000; i++) {
        char line[128];
        int n = snprintf(line, sizeof(line), "{\"sensor\":\"probe-%d\",\"celsius\":%.2f,\"ok\":true}\n", i % 16,
                         20.0 + (i % 100) * 0.1);
        if (used + (size_t)n > sizeof(chunk)) {
            if (!nyxj_sax_feed(sax, chunk, used)) break;
            total_bytes += used;
            used = 0;
        }
        memcpy(chunk + used, line, (size_t)n);
        used += (size_t)n;
    }
    bool ok = nyxj_sax_feed(sax, chunk, used) && nyxj_sax_finish(sax);
    total_bytes += used;
    if (!ok) {
        fprintf(stderr, "stream error: %s\n", nyxj_sax_error(sax)->message);
        nyxj_sax_destroy(sax);
        return 1;
    }
    printf("%llu documents, %lu bytes streamed\n", (unsigned long long)nyxj_sax_documents(sax), total_bytes);
    printf("average %.2f C, max %.2f C over %lu readings\n", stats.sum / (double)stats.readings, stats.max, stats.readings);
    printf("parser memory: %zu bytes (constant)\n", nyxj_sax_memory(sax));
    nyxj_sax_destroy(sax);
    return 0;
}
