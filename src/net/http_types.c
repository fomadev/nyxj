#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../core/internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* --- Route matching --- */

static bool params_put(nyxj_request* req, size_t* used, const char* name, size_t nlen, const char* val, size_t vlen) {
    if (!req) return true;
    if (req->param_count >= NYXJ_MAX_PARAMS || *used + nlen + vlen + 2 > sizeof(req->params_buf_)) return false;
    char* n = req->params_buf_ + *used;
    memcpy(n, name, nlen);
    n[nlen] = '\0';
    char* v = n + nlen + 1;
    memcpy(v, val, vlen);
    v[vlen] = '\0';
    *used += nlen + vlen + 2;
    req->params[req->param_count].name = n;
    req->params[req->param_count].value = v;
    req->param_count++;
    return true;
}

bool nyxj_route_match(const char* pattern, const char* path, nyxj_request* req) {
    if (!pattern || !path) return false;
    if (req) req->param_count = 0;
    size_t used = 0;
    const char* p = pattern;
    const char* s = path;
    while (*p) {
        if (*p == ':') {
            const char* name = ++p;
            while (*p && *p != '/') p++;
            const char* val = s;
            while (*s && *s != '/') s++;
            if (s == val || !params_put(req, &used, name, (size_t)(p - name), val, (size_t)(s - val))) goto nomatch;
            continue;
        }
        if (*p == '*') {
            if (!params_put(req, &used, "*", 1, s, strlen(s))) goto nomatch;
            return true;
        }
        if (*p != *s) goto nomatch;
        p++;
        s++;
    }
    if (*s == '\0' || (s[0] == '/' && s[1] == '\0')) return true; /* Tolerate a trailing slash */
nomatch:
    if (req) req->param_count = 0;
    return false;
}

/* --- Request helpers --- */

const char* nyxj_req_param(const nyxj_request* req, const char* name) {
    if (!req || !name) return NULL;
    for (int i = 0; i < req->param_count; i++)
        if (strcmp(req->params[i].name, name) == 0) return req->params[i].value;
    return NULL;
}

static bool ieq(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

const char* nyxj_req_header(const nyxj_request* req, const char* name) {
    if (!req || !name) return NULL;
    for (int i = 0; i < req->header_count; i++)
        if (ieq(req->headers[i].name, name)) return req->headers[i].value;
    return NULL;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool nyxj_req_query(const nyxj_request* req, const char* name, char* out, size_t out_size) {
    if (!req || !name || !out || !out_size || !req->query) return false;
    size_t nlen = strlen(name);
    const char* q = req->query;
    while (*q) {
        const char* amp = strchr(q, '&');
        const char* end = amp ? amp : q + strlen(q);
        const char* eq = memchr(q, '=', (size_t)(end - q));
        const char* kend = eq ? eq : end;
        if ((size_t)(kend - q) == nlen && memcmp(q, name, nlen) == 0) {
            size_t k = 0;
            for (const char* v = eq ? eq + 1 : end; v < end && k + 1 < out_size; v++) {
                if (*v == '+') out[k++] = ' ';
                else if (*v == '%' && v + 2 < end && hexval(v[1]) >= 0 && hexval(v[2]) >= 0) {
                    out[k++] = (char)(hexval(v[1]) * 16 + hexval(v[2]));
                    v += 2;
                } else out[k++] = *v;
            }
            out[k] = '\0';
            return true;
        }
        if (!amp) break;
        q = amp + 1;
    }
    return false;
}

nyxj_value* nyxj_req_json(nyxj_request* req) {
    if (!req) return NULL;
    if (req->json_parsed_) return req->json_;
    req->json_parsed_ = true;
    nyxj_parse_options opts = req->app ? req->app->cfg.parse : nyxj_parse_options_default();
    opts.arena = NULL;
    uint64_t t0 = nyxj_now_ns();
    nyxj_result r = nyxj_parse_ex(req->body ? req->body : "", req->body_len, &opts);
    double secs = (double)(nyxj_now_ns() - t0) / 1e9;
    if (req->app) {
        nyxj_metric_observe(req->app->m_parse_duration, secs);
        nyxj_metric_add(req->app->m_parsed_bytes, (double)req->body_len);
        if (!r.is_valid) nyxj_metric_inc(req->app->m_parse_errors);
    }
    req->json_ = r.root;
    req->json_error_ = r.error;
    return req->json_;
}

const nyxj_error* nyxj_req_json_error(const nyxj_request* req) { return req ? &req->json_error_ : NULL; }

/* --- Response helpers --- */

static void set_body(nyxj_response* res, const void* data, size_t len) {
    free(res->body);
    res->body = NULL;
    res->body_len = 0;
    if (!data || !len) return;
    res->body = malloc(len + 1);
    if (!res->body) return;
    memcpy(res->body, data, len);
    res->body[len] = '\0';
    res->body_len = len;
}

static void set_type(nyxj_response* res, const char* ct) {
    snprintf(res->content_type, sizeof(res->content_type), "%s", ct ? ct : "");
}

void nyxj_res_status(nyxj_response* res, int status) {
    if (res) res->status = status;
}

void nyxj_res_header(nyxj_response* res, const char* name, const char* value) {
    if (!res || !name || !value) return;
    size_t used = strlen(res->headers);
    size_t need = strlen(name) + strlen(value) + 4;
    if (used + need >= sizeof(res->headers)) return;
    char* p = res->headers + used;
    for (const char* s = name; *s; s++)
        if (*s != '\r' && *s != '\n' && *s != ':') *p++ = *s;
    *p++ = ':';
    *p++ = ' ';
    for (const char* s = value; *s; s++)
        if (*s != '\r' && *s != '\n') *p++ = *s; /* Header injection protection */
    *p++ = '\r';
    *p++ = '\n';
    *p = '\0';
}

void nyxj_res_bytes(nyxj_response* res, int status, const char* content_type, const void* data, size_t len) {
    if (!res) return;
    res->status = status;
    set_type(res, content_type);
    set_body(res, data, len);
}

void nyxj_res_text(nyxj_response* res, int status, const char* content_type, const char* text) {
    nyxj_res_bytes(res, status, content_type ? content_type : "text/plain; charset=utf-8", text, text ? strlen(text) : 0);
}

void nyxj_res_json_str(nyxj_response* res, int status, const char* json) {
    nyxj_res_bytes(res, status, "application/json", json, json ? strlen(json) : 0);
}

void nyxj_res_json(nyxj_response* res, int status, nyxj_value* body) {
    if (!res) { nyxj_free_value(body); return; }
    size_t n = 0;
    char* text = body ? nyxj_serialize_ex(body, NULL, &n) : NULL;
    nyxj_free_value(body);
    if (!text) {
        nyxj_res_json_str(res, 500, "{\"error\":\"out_of_memory\",\"message\":\"cannot serialize response\",\"status\":500}");
        return;
    }
    free(res->body);
    res->body = text;
    res->body_len = n;
    res->status = status;
    set_type(res, "application/json");
}

void nyxj_res_error(nyxj_response* res, int status, const char* code, const char* message) {
    nyxj_value* o = nyxj_new_object();
    if (o) {
        nyxj_object_set_string(o, "error", code ? code : "error");
        nyxj_object_set_string(o, "message", message ? message : "");
        nyxj_object_set_number(o, "status", status);
    }
    nyxj_res_json(res, status, o);
}
