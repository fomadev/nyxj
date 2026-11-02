/*
 * NyxJ structured logging.
 *
 * Levels can be filtered at runtime (nyxj_log_set_level) and stripped at
 * compile time: build with -DNYXJ_LOG_COMPILE_LEVEL=2 to remove TRACE and
 * DEBUG calls entirely from release binaries.
 */
#ifndef NYXJ_LOG_H
#define NYXJ_LOG_H

#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NYXJ_LOG_TRACE = 0,
    NYXJ_LOG_DEBUG = 1,
    NYXJ_LOG_INFO  = 2,
    NYXJ_LOG_WARN  = 3,
    NYXJ_LOG_ERROR = 4,
    NYXJ_LOG_FATAL = 5,
    NYXJ_LOG_OFF   = 6
} nyxj_log_level;

typedef void (*nyxj_log_sink)(nyxj_log_level level, const char* module,
                              const char* message, void* user);

void           nyxj_log_set_level(nyxj_log_level level);
nyxj_log_level nyxj_log_get_level(void);
void           nyxj_log_set_json(bool enabled);     /* One JSON object per line */
void           nyxj_log_set_stream(FILE* stream);   /* Default: stderr */
void           nyxj_log_set_sink(nyxj_log_sink sink, void* user); /* NULL restores stream output */
bool           nyxj_log_level_parse(const char* name, nyxj_log_level* out);
const char*    nyxj_log_level_name(nyxj_log_level level);

void nyxj_log_write(nyxj_log_level level, const char* module, const char* file, int line,
                    const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 5, 6)))
#endif
    ;

#ifndef NYXJ_LOG_COMPILE_LEVEL
#define NYXJ_LOG_COMPILE_LEVEL 0
#endif

#define NYXJ_LOG(level, module, ...)                                              \
    do {                                                                          \
        if ((int)(level) >= NYXJ_LOG_COMPILE_LEVEL && (level) >= nyxj_log_get_level()) \
            nyxj_log_write((level), (module), __FILE__, __LINE__, __VA_ARGS__);  \
    } while (0)

#define NYXJ_TRACE(module, ...) NYXJ_LOG(NYXJ_LOG_TRACE, module, __VA_ARGS__)
#define NYXJ_DEBUG(module, ...) NYXJ_LOG(NYXJ_LOG_DEBUG, module, __VA_ARGS__)
#define NYXJ_INFO(module, ...)  NYXJ_LOG(NYXJ_LOG_INFO, module, __VA_ARGS__)
#define NYXJ_WARN(module, ...)  NYXJ_LOG(NYXJ_LOG_WARN, module, __VA_ARGS__)
#define NYXJ_ERROR(module, ...) NYXJ_LOG(NYXJ_LOG_ERROR, module, __VA_ARGS__)
#define NYXJ_FATAL(module, ...) NYXJ_LOG(NYXJ_LOG_FATAL, module, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
