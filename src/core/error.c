#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj.h"
#include "scan.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* --- Error catalogue --- */

static const struct {
    const char* name;
    const char* message;
} error_table[NYXJ_ERR_COUNT] = {
    [NYXJ_OK]                   = {"OK", "success"},
    [NYXJ_ERR_NULL_INPUT]       = {"NULL_INPUT", "input is NULL"},
    [NYXJ_ERR_OUT_OF_MEMORY]    = {"OUT_OF_MEMORY", "out of memory"},
    [NYXJ_ERR_UNEXPECTED_EOF]   = {"UNEXPECTED_EOF", "unexpected end of input"},
    [NYXJ_ERR_UNEXPECTED_CHAR]  = {"UNEXPECTED_CHAR", "unexpected character"},
    [NYXJ_ERR_INVALID_LITERAL]  = {"INVALID_LITERAL", "invalid literal (expected true, false or null)"},
    [NYXJ_ERR_INVALID_NUMBER]   = {"INVALID_NUMBER", "invalid number"},
    [NYXJ_ERR_INVALID_STRING]   = {"INVALID_STRING", "invalid or unterminated string"},
    [NYXJ_ERR_INVALID_ESCAPE]   = {"INVALID_ESCAPE", "invalid escape sequence"},
    [NYXJ_ERR_INVALID_UNICODE]  = {"INVALID_UNICODE", "invalid \\u escape or surrogate pair"},
    [NYXJ_ERR_INVALID_UTF8]     = {"INVALID_UTF8", "invalid UTF-8 byte sequence"},
    [NYXJ_ERR_CONTROL_CHAR]     = {"CONTROL_CHAR", "unescaped control character in string"},
    [NYXJ_ERR_TRAILING_DATA]    = {"TRAILING_DATA", "unexpected data after the JSON value"},
    [NYXJ_ERR_DUPLICATE_KEY]    = {"DUPLICATE_KEY", "duplicate object key"},
    [NYXJ_ERR_DEPTH_LIMIT]      = {"DEPTH_LIMIT", "maximum nesting depth exceeded"},
    [NYXJ_ERR_SIZE_LIMIT]       = {"SIZE_LIMIT", "input exceeds the maximum allowed size"},
    [NYXJ_ERR_STRING_LIMIT]     = {"STRING_LIMIT", "string exceeds the maximum allowed length"},
    [NYXJ_ERR_ELEMENT_LIMIT]    = {"ELEMENT_LIMIT", "container exceeds the maximum number of elements"},
    [NYXJ_ERR_TYPE_MISMATCH]    = {"TYPE_MISMATCH", "value has an unexpected type"},
    [NYXJ_ERR_NOT_FOUND]        = {"NOT_FOUND", "value not found"},
    [NYXJ_ERR_MISSING_FIELD]    = {"MISSING_FIELD", "required field is missing"},
    [NYXJ_ERR_SCHEMA_VIOLATION] = {"SCHEMA_VIOLATION", "document does not match the schema"},
    [NYXJ_ERR_INVALID_SCHEMA]   = {"INVALID_SCHEMA", "invalid schema"},
    [NYXJ_ERR_INVALID_QUERY]    = {"INVALID_QUERY", "invalid query expression"},
    [NYXJ_ERR_INVALID_POINTER]  = {"INVALID_POINTER", "invalid JSON pointer"},
    [NYXJ_ERR_PATCH_FAILED]     = {"PATCH_FAILED", "patch operation failed"},
    [NYXJ_ERR_TEST_FAILED]      = {"TEST_FAILED", "patch test operation failed"},
    [NYXJ_ERR_MSGPACK]          = {"MSGPACK", "invalid MessagePack data"},
    [NYXJ_ERR_INTEGRITY]        = {"INTEGRITY", "integrity check failed (shadow execution mismatch)"},
    [NYXJ_ERR_IO]               = {"IO", "input/output error"},
    [NYXJ_ERR_NETWORK]          = {"NETWORK", "network error"},
    [NYXJ_ERR_TIMEOUT]          = {"TIMEOUT", "operation timed out"},
    [NYXJ_ERR_TLS]              = {"TLS", "TLS error"},
    [NYXJ_ERR_QUEUE_FULL]       = {"QUEUE_FULL", "work queue is full"},
    [NYXJ_ERR_PLUGIN]           = {"PLUGIN", "plugin error"},
    [NYXJ_ERR_UNSUPPORTED]      = {"UNSUPPORTED", "operation not supported on this platform"},
    [NYXJ_ERR_INVALID_ARGUMENT] = {"INVALID_ARGUMENT", "invalid argument"},
};

const char* nyxj_strerror(nyxj_error_code code) {
    if ((int)code < 0 || code >= NYXJ_ERR_COUNT) return "unknown error";
    return error_table[code].message;
}

const char* nyxj_error_name(nyxj_error_code code) {
    if ((int)code < 0 || code >= NYXJ_ERR_COUNT) return "UNKNOWN";
    return error_table[code].name;
}

void nyxj_error_clear(nyxj_error* err) {
    if (!err) return;
    memset(err, 0, sizeof(*err));
}

void nyxj_error_set(nyxj_error* err, nyxj_error_code code, const char* fmt, ...) {
    if (!err) return;
    err->code = code;
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err->message, sizeof(err->message), fmt, ap);
        va_end(ap);
    } else {
        snprintf(err->message, sizeof(err->message), "%s", nyxj_strerror(code));
    }
}

/* --- File helper --- */

char* nyxj_read_file(const char* path, size_t* out_len, nyxj_error* err) {
    if (out_len) *out_len = 0;
    if (!path) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return NULL; }
    FILE* f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!f) {
        nyxj_error_set(err, NYXJ_ERR_IO, "cannot open '%s': %s", path, strerror(errno));
        return NULL;
    }
    size_t cap = 65536, len = 0;
    char* data = malloc(cap);
    while (data) {
        size_t n = fread(data + len, 1, cap - len - 1, f);
        len += n;
        if (n == 0) break;
        if (cap - len - 1 == 0) {
            char* grown = realloc(data, cap * 2);
            if (!grown) { free(data); data = NULL; break; }
            data = grown;
            cap *= 2;
        }
    }
    bool failed = ferror(f) != 0;
    if (f != stdin) fclose(f);
    if (!data) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    if (failed) {
        free(data);
        nyxj_error_set(err, NYXJ_ERR_IO, "failed to read '%s'", path);
        return NULL;
    }
    data[len] = '\0';
    if (out_len) *out_len = len;
    return data;
}

/* --- Library information --- */

const char* nyxj_version(void) { return NYXJ_VERSION; }

const char* nyxj_simd_backend(void) {
#if defined(NYXJ_SIMD_SSE2)
    return "sse2";
#elif defined(NYXJ_SIMD_NEON)
    return "neon";
#else
    return "scalar";
#endif
}

const char* nyxj_build_features(void) {
    return "core,schema,query,patch,msgpack,view,sax,repair,gate,reliability,arena"
#if !defined(NYXJ_NO_THREADS)
           ",threads"
#endif
#if defined(NYXJ_SIMD_SSE2)
           ",simd-sse2"
#elif defined(NYXJ_SIMD_NEON)
           ",simd-neon"
#endif
#if defined(NYXJ_WITH_NET)
           ",http,router,client,tls,metrics,kv,plugins,ipc,cluster"
#endif
        ;
}
