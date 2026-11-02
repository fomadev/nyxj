#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj_log.h"
#include "../common/buf.h"
#include "../common/thread.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

static volatile nyxj_log_level g_level = NYXJ_LOG_INFO;
static bool g_json = false;
static FILE* g_stream = NULL;
static nyxj_log_sink g_sink = NULL;
static void* g_sink_user = NULL;
static nyxj_mutex g_lock = NYXJ_MUTEX_INITIALIZER;

static const char* level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};

void nyxj_log_set_level(nyxj_log_level level) { g_level = level; }
nyxj_log_level nyxj_log_get_level(void) { return g_level; }
void nyxj_log_set_json(bool enabled) { g_json = enabled; }
void nyxj_log_set_stream(FILE* stream) { g_stream = stream; }

void nyxj_log_set_sink(nyxj_log_sink sink, void* user) {
    nyxj_mutex_lock(&g_lock);
    g_sink = sink;
    g_sink_user = user;
    nyxj_mutex_unlock(&g_lock);
}

const char* nyxj_log_level_name(nyxj_log_level level) {
    if ((int)level < 0 || level > NYXJ_LOG_OFF) return "UNKNOWN";
    return level_names[level];
}

bool nyxj_log_level_parse(const char* name, nyxj_log_level* out) {
    if (!name) return false;
    for (int i = 0; i <= NYXJ_LOG_OFF; i++) {
        const char* a = name;
        const char* b = level_names[i];
        while (*a && *b && ((*a | 0x20) == (*b | 0x20))) { a++; b++; }
        if (!*a && !*b) { *out = (nyxj_log_level)i; return true; }
    }
    if (strcmp(name, "warning") == 0) { *out = NYXJ_LOG_WARN; return true; }
    return false;
}

static void format_time(char* out, size_t n) {
    uint64_t ms = nyxj_wall_ms();
    time_t secs = (time_t)(ms / 1000);
    struct tm tmv;
#if defined(_WIN32)
    gmtime_s(&tmv, &secs);
#else
    gmtime_r(&secs, &tmv);
#endif
    size_t k = strftime(out, n, "%Y-%m-%dT%H:%M:%S", &tmv);
    snprintf(out + k, n - k, ".%03uZ", (unsigned)(ms % 1000));
}

void nyxj_log_write(nyxj_log_level level, const char* module, const char* file, int line,
                    const char* fmt, ...) {
    if (level < g_level || level >= NYXJ_LOG_OFF) return;
    char message[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);
    if (!module) module = "nyxj";

    nyxj_mutex_lock(&g_lock);
    if (g_sink) {
        g_sink(level, module, message, g_sink_user);
        nyxj_mutex_unlock(&g_lock);
        return;
    }
    char ts[40];
    format_time(ts, sizeof(ts));
    FILE* out = g_stream ? g_stream : stderr;
    const char* base = file;
    if (file) {
        for (const char* p = file; *p; p++)
            if (*p == '/' || *p == '\\') base = p + 1;
    }
    if (g_json) {
        nyxj_buf b;
        nyxj_buf_init(&b, 256);
        nyxj_buf_printf(&b, "{\"ts\":\"%s\",\"level\":\"%s\",\"module\":", ts, level_names[level]);
        nyxj_buf_json_string(&b, module, false);
        nyxj_buf_puts(&b, ",\"msg\":");
        nyxj_buf_json_string(&b, message, false);
        if (base) nyxj_buf_printf(&b, ",\"src\":\"%s:%d\"", base, line);
        nyxj_buf_putc(&b, '}');
        if (b.data) fprintf(out, "%s\n", b.data);
        nyxj_buf_free(&b);
    } else if (level >= NYXJ_LOG_WARN && base) {
        fprintf(out, "%s %-5s [%s] %s (%s:%d)\n", ts, level_names[level], module, message, base, line);
    } else {
        fprintf(out, "%s %-5s [%s] %s\n", ts, level_names[level], module, message);
    }
    fflush(out);
    nyxj_mutex_unlock(&g_lock);
}
