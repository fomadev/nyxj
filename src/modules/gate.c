#include "../../include/nyxj_gate.h"
#include "../../include/nyxj_sax.h"
#include "../core/scan.h"

#include <string.h>

nyxj_gate_policy nyxj_gate_policy_default(void) {
    nyxj_gate_policy p;
    memset(&p, 0, sizeof(p));
    p.max_size = 1024 * 1024;
    p.max_depth = 64;
    p.max_string_length = 64 * 1024;
    p.max_elements = 10000;
    return p;
}

nyxj_gate_policy nyxj_gate_policy_strict(void) {
    nyxj_gate_policy p;
    memset(&p, 0, sizeof(p));
    p.max_size = 64 * 1024;
    p.max_depth = 16;
    p.max_string_length = 4096;
    p.max_elements = 1000;
    p.require_object_root = true;
    return p;
}

static const char* reason_for(nyxj_error_code code) {
    switch (code) {
        case NYXJ_ERR_SIZE_LIMIT: return "size_limit";
        case NYXJ_ERR_DEPTH_LIMIT: return "depth_limit";
        case NYXJ_ERR_STRING_LIMIT: return "string_limit";
        case NYXJ_ERR_ELEMENT_LIMIT: return "element_limit";
        case NYXJ_ERR_INVALID_UTF8: return "invalid_utf8";
        case NYXJ_ERR_CONTROL_CHAR: return "control_char";
        case NYXJ_ERR_TYPE_MISMATCH: return "root_type";
        case NYXJ_ERR_UNEXPECTED_EOF: return "empty_or_truncated";
        default: return "malformed";
    }
}

static bool deny(nyxj_gate_verdict* v, nyxj_error_code code, const char* msg) {
    v->allowed = false;
    if (v->error.code == NYXJ_OK) nyxj_error_set(&v->error, code, "%s", msg ? msg : nyxj_strerror(code));
    v->reason = reason_for(v->error.code);
    return false;
}

bool nyxj_gate_check(const char* buf, size_t len, const nyxj_gate_policy* policy, nyxj_gate_verdict* verdict) {
    nyxj_gate_verdict local;
    nyxj_gate_verdict* v = verdict ? verdict : &local;
    memset(v, 0, sizeof(*v));
    nyxj_gate_policy p = policy ? *policy : nyxj_gate_policy_default();

    if (!buf && len) return deny(v, NYXJ_ERR_NULL_INPUT, NULL);
    if (p.max_size && len > p.max_size) return deny(v, NYXJ_ERR_SIZE_LIMIT, "payload exceeds the maximum size");

    size_t start = 0;
    if (len >= 3 && memcmp(buf, "\xEF\xBB\xBF", 3) == 0) start = 3;
    v->body_offset = start;

    const char* body = buf + start;
    size_t blen = len - start;
    const char* first = nyxj_scan_ws(body, body + blen);
    if (first == body + blen) {
        if (p.allow_empty) { v->allowed = true; v->reason = "ok"; return true; }
        return deny(v, NYXJ_ERR_UNEXPECTED_EOF, "empty payload");
    }
    if (memchr(body, '\0', blen)) return deny(v, NYXJ_ERR_CONTROL_CHAR, "payload contains NUL bytes");
    if (p.require_object_root && *first != '{') return deny(v, NYXJ_ERR_TYPE_MISMATCH, "payload root must be a JSON object");

    nyxj_sax_options o;
    memset(&o, 0, sizeof(o));
    o.max_depth = p.max_depth > 0 ? p.max_depth : NYXJ_DEFAULT_MAX_DEPTH;
    o.max_token_size = p.max_string_length ? p.max_string_length : 64 * 1024;
    o.max_elements = p.max_elements;
    nyxj_error err;
    if (!nyxj_sax_validate(body, blen, &o, &err)) {
        v->error = err;
        if (v->error.code == NYXJ_OK) v->error.code = NYXJ_ERR_UNEXPECTED_CHAR;
        v->error.offset += start;
        return deny(v, v->error.code, NULL);
    }
    v->allowed = true;
    v->reason = "ok";
    return true;
}
