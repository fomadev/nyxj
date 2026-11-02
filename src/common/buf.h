/*
 * Internal growable byte buffer used by the serializer, repair engine,
 * metrics exposition, OpenAPI generator and MessagePack encoder.
 */
#ifndef NYXJ_BUF_H
#define NYXJ_BUF_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char* data;
    size_t len;
    size_t cap;
    bool oom; /* Sticky out-of-memory flag: later writes become no-ops */
} nyxj_buf;

void  nyxj_buf_init(nyxj_buf* b, size_t initial);
bool  nyxj_buf_reserve(nyxj_buf* b, size_t extra);
void  nyxj_buf_putc(nyxj_buf* b, char c);
void  nyxj_buf_append(nyxj_buf* b, const void* data, size_t n);
void  nyxj_buf_puts(nyxj_buf* b, const char* s);
void  nyxj_buf_printf(nyxj_buf* b, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void  nyxj_buf_json_string(nyxj_buf* b, const char* s, bool escape_unicode);
void  nyxj_buf_json_number(nyxj_buf* b, double n);
char* nyxj_buf_detach(nyxj_buf* b, size_t* out_len); /* NUL-terminated, NULL on OOM */
void  nyxj_buf_free(nyxj_buf* b);

#endif
