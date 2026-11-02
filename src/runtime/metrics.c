#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_metrics.h"
#include "../common/buf.h"
#include "../common/thread.h"
#include "../core/internal.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <stdlib.h>
#include <sys/resource.h>
#elif !defined(__EMSCRIPTEN__)
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

typedef enum { M_COUNTER, M_GAUGE, M_HISTOGRAM } mtype;

struct nyxj_metric {
    nyxj_metrics* owner;
    mtype type;
    char* name;
    char* help;
    char* labels;
    double value;
    /* Histogram */
    double* bounds;
    uint64_t* counts;
    int nbuckets;
    double sum;
    uint64_t count;
    struct nyxj_metric* next;
};

typedef struct collector {
    nyxj_metrics_collector fn;
    void* user;
    struct collector* next;
} collector;

struct nyxj_metrics {
    nyxj_mutex lock;
    nyxj_metric* head;
    nyxj_metric* tail;
    collector* collectors;
};

static const double default_buckets[] = {0.0005, 0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10};

nyxj_metrics* nyxj_metrics_create(void) {
    nyxj_metrics* m = calloc(1, sizeof(*m));
    if (m) nyxj_mutex_init(&m->lock);
    return m;
}

void nyxj_metrics_destroy(nyxj_metrics* m) {
    if (!m) return;
    nyxj_metric* s = m->head;
    while (s) {
        nyxj_metric* n = s->next;
        free(s->name);
        free(s->help);
        free(s->labels);
        free(s->bounds);
        free(s->counts);
        free(s);
        s = n;
    }
    collector* c = m->collectors;
    while (c) { collector* n = c->next; free(c); c = n; }
    nyxj_mutex_destroy(&m->lock);
    free(m);
}

static nyxj_metric* get_or_create(nyxj_metrics* m, mtype type, const char* name, const char* help, const char* labels,
                                  const double* buckets, int nbuckets) {
    if (!m || !name) return NULL;
    if (!labels) labels = "";
    nyxj_mutex_lock(&m->lock);
    for (nyxj_metric* s = m->head; s; s = s->next) {
        if (strcmp(s->name, name) == 0 && strcmp(s->labels, labels) == 0) {
            nyxj_mutex_unlock(&m->lock);
            return s->type == type ? s : NULL;
        }
    }
    nyxj_metric* s = calloc(1, sizeof(*s));
    if (s) {
        s->owner = m;
        s->type = type;
        s->name = nyxj_strdup(name);
        s->help = nyxj_strdup(help ? help : "");
        s->labels = nyxj_strdup(labels);
        if (type == M_HISTOGRAM) {
            if (!buckets || nbuckets <= 0) {
                buckets = default_buckets;
                nbuckets = (int)(sizeof(default_buckets) / sizeof(default_buckets[0]));
            }
            s->nbuckets = nbuckets;
            s->bounds = malloc(sizeof(double) * (size_t)nbuckets);
            s->counts = calloc((size_t)nbuckets, sizeof(uint64_t));
            if (s->bounds) memcpy(s->bounds, buckets, sizeof(double) * (size_t)nbuckets);
        }
        if (!s->name || !s->help || !s->labels || (type == M_HISTOGRAM && (!s->bounds || !s->counts))) {
            free(s->name); free(s->help); free(s->labels); free(s->bounds); free(s->counts); free(s);
            s = NULL;
        } else {
            if (m->tail) m->tail->next = s;
            else m->head = s;
            m->tail = s;
        }
    }
    nyxj_mutex_unlock(&m->lock);
    return s;
}

nyxj_metric* nyxj_metrics_counter(nyxj_metrics* m, const char* name, const char* help, const char* labels) {
    return get_or_create(m, M_COUNTER, name, help, labels, NULL, 0);
}

nyxj_metric* nyxj_metrics_gauge(nyxj_metrics* m, const char* name, const char* help, const char* labels) {
    return get_or_create(m, M_GAUGE, name, help, labels, NULL, 0);
}

nyxj_metric* nyxj_metrics_histogram(nyxj_metrics* m, const char* name, const char* help, const char* labels,
                                    const double* buckets, int nbuckets) {
    return get_or_create(m, M_HISTOGRAM, name, help, labels, buckets, nbuckets);
}

void nyxj_metric_add(nyxj_metric* s, double delta) {
    if (!s) return;
    nyxj_mutex_lock(&s->owner->lock);
    if (s->type == M_COUNTER && delta < 0) delta = 0; /* Counters are monotonic */
    s->value += delta;
    nyxj_mutex_unlock(&s->owner->lock);
}

void nyxj_metric_inc(nyxj_metric* s) { nyxj_metric_add(s, 1.0); }

void nyxj_metric_set(nyxj_metric* s, double value) {
    if (!s) return;
    nyxj_mutex_lock(&s->owner->lock);
    s->value = value;
    nyxj_mutex_unlock(&s->owner->lock);
}

void nyxj_metric_observe(nyxj_metric* s, double value) {
    if (!s || s->type != M_HISTOGRAM) return;
    nyxj_mutex_lock(&s->owner->lock);
    for (int i = 0; i < s->nbuckets; i++)
        if (value <= s->bounds[i]) { s->counts[i]++; break; }
    s->sum += value;
    s->count++;
    nyxj_mutex_unlock(&s->owner->lock);
}

double nyxj_metric_value(nyxj_metric* s) {
    if (!s) return 0;
    nyxj_mutex_lock(&s->owner->lock);
    double v = s->type == M_HISTOGRAM ? (double)s->count : s->value;
    nyxj_mutex_unlock(&s->owner->lock);
    return v;
}

void nyxj_metrics_add_collector(nyxj_metrics* m, nyxj_metrics_collector fn, void* user) {
    if (!m || !fn) return;
    collector* c = calloc(1, sizeof(*c));
    if (!c) return;
    c->fn = fn;
    c->user = user;
    nyxj_mutex_lock(&m->lock);
    c->next = m->collectors;
    m->collectors = c;
    nyxj_mutex_unlock(&m->lock);
}

void nyxj_metrics_escape_label(const char* value, char* out, size_t out_size) {
    size_t k = 0;
    if (!out_size) return;
    for (const char* p = value ? value : ""; *p && k + 3 < out_size; p++) {
        if (*p == '\\' || *p == '"') { out[k++] = '\\'; out[k++] = *p; }
        else if (*p == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else out[k++] = *p;
    }
    out[k] = '\0';
}

static void put_number(nyxj_buf* b, double v) {
    if (v != v) nyxj_buf_puts(b, "NaN");
    else if (v > 1.7976931348623157e308) nyxj_buf_puts(b, "+Inf");
    else if (v < -1.7976931348623157e308) nyxj_buf_puts(b, "-Inf");
    else nyxj_buf_json_number(b, v);
}

static void put_series_name(nyxj_buf* b, const char* name, const char* suffix, const char* labels, const char* extra) {
    nyxj_buf_puts(b, name);
    nyxj_buf_puts(b, suffix);
    bool has_labels = labels && *labels;
    if (has_labels || extra) {
        nyxj_buf_putc(b, '{');
        if (has_labels) nyxj_buf_puts(b, labels);
        if (has_labels && extra) nyxj_buf_putc(b, ',');
        if (extra) nyxj_buf_puts(b, extra);
        nyxj_buf_putc(b, '}');
    }
    nyxj_buf_putc(b, ' ');
}

char* nyxj_metrics_render(nyxj_metrics* m) {
    if (!m) return NULL;
    for (collector* c = m->collectors; c; c = c->next) c->fn(m, c->user);
    nyxj_buf b;
    nyxj_buf_init(&b, 4096);
    nyxj_mutex_lock(&m->lock);
    for (nyxj_metric* s = m->head; s; s = s->next) {
        bool first = true;
        for (nyxj_metric* e = m->head; e != s; e = e->next)
            if (strcmp(e->name, s->name) == 0) { first = false; break; }
        if (!first) continue;
        static const char* type_names[] = {"counter", "gauge", "histogram"};
        nyxj_buf_printf(&b, "# HELP %s %s\n# TYPE %s %s\n", s->name, s->help, s->name, type_names[s->type]);
        for (nyxj_metric* e = s; e; e = e->next) {
            if (strcmp(e->name, s->name) != 0) continue;
            if (e->type != M_HISTOGRAM) {
                put_series_name(&b, e->name, "", e->labels, NULL);
                put_number(&b, e->value);
                nyxj_buf_putc(&b, '\n');
                continue;
            }
            uint64_t cum = 0;
            char le[48];
            for (int i = 0; i < e->nbuckets; i++) {
                cum += e->counts[i];
                snprintf(le, sizeof(le), "le=\"%g\"", e->bounds[i]);
                put_series_name(&b, e->name, "_bucket", e->labels, le);
                nyxj_buf_printf(&b, "%llu\n", (unsigned long long)cum);
            }
            put_series_name(&b, e->name, "_bucket", e->labels, "le=\"+Inf\"");
            nyxj_buf_printf(&b, "%llu\n", (unsigned long long)e->count);
            put_series_name(&b, e->name, "_sum", e->labels, NULL);
            put_number(&b, e->sum);
            nyxj_buf_putc(&b, '\n');
            put_series_name(&b, e->name, "_count", e->labels, NULL);
            nyxj_buf_printf(&b, "%llu\n", (unsigned long long)e->count);
        }
    }
    nyxj_mutex_unlock(&m->lock);
    return nyxj_buf_detach(&b, NULL);
}

/* --- Resource monitoring --- */

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
static double read_cpu_temperature(void) {
    double best = -1;
    for (int i = 0; i < 16; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        FILE* f = fopen(path, "r");
        if (!f) continue;
        long milli = 0;
        if (fscanf(f, "%ld", &milli) == 1 && milli / 1000.0 > best) best = milli / 1000.0;
        fclose(f);
    }
    return best;
}
#endif

bool nyxj_sys_stats_get(nyxj_sys_stats* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->load1 = -1;
    out->cpu_temp_c = -1;
    out->cpu_count = nyxj_cpu_count();
#if defined(_WIN32)
    FILETIME create, exit_t, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &create, &exit_t, &kernel, &user)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        out->cpu_seconds = (double)(k.QuadPart + u.QuadPart) / 1e7;
    }
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) out->rss_bytes = pmc.WorkingSetSize;
    return true;
#elif defined(__EMSCRIPTEN__)
    return false;
#else
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        out->cpu_seconds = (double)ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 + (double)ru.ru_stime.tv_sec +
                           ru.ru_stime.tv_usec / 1e6;
    double load[1];
    if (getloadavg(load, 1) == 1) out->load1 = load[0];
#if defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        out->rss_bytes = info.resident_size;
#else
    FILE* f = fopen("/proc/self/statm", "r");
    if (f) {
        unsigned long size = 0, resident = 0;
        if (fscanf(f, "%lu %lu", &size, &resident) == 2) out->rss_bytes = (uint64_t)resident * (uint64_t)sysconf(_SC_PAGESIZE);
        fclose(f);
    }
    out->cpu_temp_c = read_cpu_temperature();
#endif
    return true;
#endif
}
