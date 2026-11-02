#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../core/internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Synchronous HTTP/HTTPS client: each call runs its own short-lived event
 * loop, so it is safe to use from any thread (including pool workers). */

typedef struct {
    const nyxj_http_options* o;
    const char* method;
    nyxj_http_response* resp;
    nyxj_error* err;
    char* ca;
    size_t ca_len;
    bool done;
} client_state;

static struct mg_str url_authority(const char* url) {
    const char* p = strstr(url, "://");
    p = p ? p + 3 : url;
    const char* e = p;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    return mg_str_n(p, (size_t)(e - p));
}

static void client_fn(struct mg_connection* c, int ev, void* ev_data) {
    client_state* st = c->fn_data;
    if (ev == MG_EV_CONNECT) {
        if (mg_url_is_ssl(st->o->url)) {
            struct mg_tls_opts opts;
            memset(&opts, 0, sizeof(opts));
            if (st->ca) {
                opts.ca = mg_str_n(st->ca, st->ca_len);
                opts.name = mg_url_host(st->o->url); /* Verify the certificate and the host name */
            } else {
                opts.skip_verification = 1; /* No CA bundle given: encrypt without verifying */
            }
            mg_tls_init(c, &opts);
        }
        struct mg_str host = url_authority(st->o->url);
        const char* uri = mg_url_uri(st->o->url);
        mg_printf(c, "%s %s HTTP/1.1\r\nHost: %.*s\r\nConnection: close\r\nUser-Agent: NyxJ/%s\r\nAccept: */*\r\n",
                  st->method, uri && *uri ? uri : "/", (int)host.len, host.buf, NYXJ_VERSION);
        bool has_body = st->o->body_len > 0 || strcmp(st->method, "POST") == 0 || strcmp(st->method, "PUT") == 0 ||
                        strcmp(st->method, "PATCH") == 0;
        if (has_body)
            mg_printf(c, "Content-Type: %s\r\nContent-Length: %lu\r\n",
                      st->o->content_type ? st->o->content_type : "application/json", (unsigned long)st->o->body_len);
        mg_printf(c, "%s\r\n", st->o->headers ? st->o->headers : "");
        if (st->o->body_len) mg_send(c, st->o->body, st->o->body_len);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = ev_data;
        st->resp->status = mg_http_status(hm);
        const char* hs = hm->head.buf;
        const char* he = hm->head.buf + hm->head.len;
        const char* nl = memchr(hs, '\n', (size_t)(he - hs));
        if (nl) hs = nl + 1;
        st->resp->headers = nyxj_strndup(hs, (size_t)(he - hs));
        st->resp->body = nyxj_strndup(hm->body.buf, hm->body.len);
        st->resp->body_len = hm->body.len;
        if (!st->resp->headers || !st->resp->body) nyxj_error_set(st->err, NYXJ_ERR_OUT_OF_MEMORY, NULL);
        st->done = true;
        c->is_draining = 1;
    } else if (ev == MG_EV_ERROR) {
        if (!st->done) {
            const char* msg = ev_data ? (const char*)ev_data : "connection error";
            bool tls = strstr(msg, "TLS") || strstr(msg, "tls") || strstr(msg, "cert");
            nyxj_error_set(st->err, tls ? NYXJ_ERR_TLS : NYXJ_ERR_NETWORK, "%s", msg);
        }
        st->done = true;
    } else if (ev == MG_EV_CLOSE) {
        if (!st->done) nyxj_error_set(st->err, NYXJ_ERR_NETWORK, "connection closed before a response was received");
        st->done = true;
    }
}

bool nyxj_http_request(const nyxj_http_options* o, nyxj_http_response* resp, nyxj_error* err) {
    nyxj_error local;
    if (!err) err = &local;
    nyxj_error_clear(err);
    if (resp) memset(resp, 0, sizeof(*resp));
    if (!o || !o->url || !resp) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return false; }
    if (strncmp(o->url, "http://", 7) != 0 && strncmp(o->url, "https://", 8) != 0) {
        nyxj_error_set(err, NYXJ_ERR_INVALID_ARGUMENT, "URL must start with http:// or https://");
        return false;
    }
    client_state st;
    memset(&st, 0, sizeof(st));
    st.o = o;
    st.method = o->method ? o->method : (o->body_len ? "POST" : "GET");
    st.resp = resp;
    st.err = err;
    if (o->ca_file) {
        st.ca = nyxj_read_file(o->ca_file, &st.ca_len, err);
        if (!st.ca) return false;
    }
    struct mg_mgr mgr;
    nyxj_sync_mg_log();
    mg_mgr_init(&mgr);
    struct mg_connection* c = mg_http_connect(&mgr, o->url, client_fn, &st);
    if (!c) {
        nyxj_error_set(err, NYXJ_ERR_NETWORK, "cannot connect to %s", o->url);
        st.done = true;
    }
    int timeout = o->timeout_ms > 0 ? o->timeout_ms : 10000;
    uint64_t deadline = nyxj_now_ms() + (uint64_t)timeout;
    while (!st.done && nyxj_now_ms() < deadline) mg_mgr_poll(&mgr, 20);
    if (!st.done) nyxj_error_set(err, NYXJ_ERR_TIMEOUT, "no response from %s within %d ms", o->url, timeout);
    st.done = true; /* Ignore events fired while closing */
    mg_mgr_free(&mgr);
    free(st.ca);
    if (err->code != NYXJ_OK) {
        nyxj_http_response_free(resp);
        return false;
    }
    return true;
}

bool nyxj_http_get(const char* url, int timeout_ms, nyxj_http_response* resp, nyxj_error* err) {
    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    o.method = "GET";
    o.url = url;
    o.timeout_ms = timeout_ms;
    return nyxj_http_request(&o, resp, err);
}

bool nyxj_http_post_json(const char* url, const nyxj_value* body, int timeout_ms, nyxj_http_response* resp,
                         nyxj_error* err) {
    size_t n = 0;
    char* text = body ? nyxj_serialize_ex(body, NULL, &n) : NULL;
    nyxj_http_options o;
    memset(&o, 0, sizeof(o));
    o.method = "POST";
    o.url = url;
    o.body = text;
    o.body_len = n;
    o.content_type = "application/json";
    o.timeout_ms = timeout_ms;
    bool ok = nyxj_http_request(&o, resp, err);
    free(text);
    return ok;
}

nyxj_value* nyxj_http_response_json(const nyxj_http_response* resp, nyxj_error* err) {
    if (!resp || !resp->body) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return NULL; }
    nyxj_result r = nyxj_parse_n(resp->body, resp->body_len);
    if (!r.is_valid && err) *err = r.error;
    return r.root;
}

bool nyxj_http_response_header(const nyxj_http_response* resp, const char* name, char* out, size_t out_size) {
    if (!resp || !resp->headers || !name || !out || !out_size) return false;
    size_t nlen = strlen(name);
    for (const char* line = resp->headers; *line;) {
        const char* eol = strchr(line, '\n');
        size_t len = eol ? (size_t)(eol - line) : strlen(line);
        if (len > nlen && line[nlen] == ':') {
            bool match = true;
            for (size_t i = 0; i < nlen && match; i++)
                match = tolower((unsigned char)line[i]) == tolower((unsigned char)name[i]);
            if (match) {
                const char* v = line + nlen + 1;
                while (*v == ' ' || *v == '\t') v++;
                size_t vl = (size_t)(line + len - v);
                while (vl && (v[vl - 1] == '\r' || v[vl - 1] == ' ')) vl--;
                if (vl >= out_size) vl = out_size - 1;
                memcpy(out, v, vl);
                out[vl] = '\0';
                return true;
            }
        }
        if (!eol) break;
        line = eol + 1;
    }
    return false;
}

void nyxj_http_response_free(nyxj_http_response* resp) {
    if (!resp) return;
    free(resp->headers);
    free(resp->body);
    resp->headers = NULL;
    resp->body = NULL;
    resp->body_len = 0;
}
