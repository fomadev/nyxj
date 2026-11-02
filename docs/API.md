# NyxJ API guide

Every module has its own header in `include/`. The core header `nyxj.h` is all you need for parsing and
building documents. Link with `build/libnyxj.a`.

## Core (`nyxj.h`)

### Parsing and errors

```c
nyxj_result r = nyxj_parse(json);                  /* NUL-terminated input */
nyxj_result r = nyxj_parse_n(buf, len);            /* Explicit length, buffer need not be NUL-terminated */

nyxj_parse_options o = nyxj_parse_options_default();
o.max_depth = 64;                                  /* JSON DoS protection */
o.max_input_size = 1 << 20;
o.max_string_length = 4096;
o.max_elements = 10000;
o.flags = NYXJ_PARSE_RELAXED | NYXJ_PARSE_REJECT_DUPLICATE_KEYS;
nyxj_result r = nyxj_parse_ex(buf, len, &o);

if (!r.is_valid) {
    /* r.error.code, r.error.line, r.error.column, r.error.offset, r.error.message */
    fprintf(stderr, "%s [%s]\n", r.error.message, nyxj_error_name(r.error.code));
}
nyxj_free_value(r.root);
```

The parser keeps no global state: any number of threads can parse at the same time.

### Reading values

```c
nyxj_get_string(obj, "name");                      /* NULL when absent or not a string */
nyxj_get_number(obj, "price");  nyxj_get_int(obj, "id");  nyxj_get_bool(obj, "active");
nyxj_get_path(root, "orders[0].items[-1].sku");    /* Dot path, negative indexes from the end */
nyxj_path_string(root, "user.name", "anonymous");  /* With a fallback */
nyxj_array_get(arr, i);  nyxj_size(v);  nyxj_type_name(v->type);
```

### Building and transforming

```c
nyxj_value* o = nyxj_new_object();
nyxj_object_set_string(o, "name", "NyxJ");
nyxj_value* tags = nyxj_new_array();
nyxj_array_push(tags, nyxj_new_string("fast"));
nyxj_object_set(o, "tags", tags);                  /* Takes ownership */

char* compact = nyxj_serialize(o);
char* pretty = nyxj_serialize_pretty(o, 2);
nyxj_serialize_options so = {.indent = 0, .sort_keys = true, .escape_unicode = true};
char* canonical = nyxj_serialize_ex(o, &so, NULL);

nyxj_value* copy = nyxj_clone(o);
bool same = nyxj_equals(o, copy);
```

### Arena allocator

```c
nyxj_arena* a = nyxj_arena_create(64 * 1024);
nyxj_parse_options o = nyxj_parse_options_default();
o.arena = a;
for (each message) {
    nyxj_result r = nyxj_parse_ex(msg, len, &o);   /* No per-node malloc */
    handle(r.root);
    nyxj_arena_reset(a);                           /* Release everything at once */
}
nyxj_arena_destroy(a);
```

Arena trees are read-only and must not be passed to `nyxj_free_value`.

## Logging (`nyxj_log.h`)

```c
nyxj_log_set_level(NYXJ_LOG_DEBUG);
nyxj_log_set_json(true);                           /* {"ts":...,"level":"INFO","module":...,"msg":...} */
NYXJ_INFO("orders", "processed %d orders", n);
```

Compile with `-DNYXJ_LOG_COMPILE_LEVEL=2` to strip TRACE and DEBUG calls from release builds.

## Struct mapping (`nyxj_map.h`)

```c
typedef struct { int id; char* name; double score; } User;
static const nyxj_field user_fields[] = {
    NYXJ_FIELD_INT(User, id, "id", true),
    NYXJ_FIELD_STRING(User, name, "name", true),
    NYXJ_FIELD_DOUBLE(User, score, "score", false),
};
User u = {0};
nyxj_error err;
if (nyxj_map_parse(json, user_fields, NYXJ_FIELDS_COUNT(user_fields), &u, &err)) { ... }
char* back = nyxj_struct_to_json(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields), 2);
nyxj_map_free(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields));
```

Field kinds: `INT`, `INT64`, `DOUBLE`, `FLOAT`, `BOOL`, `STRING` (heap copy), `STRBUF` (fixed buffer),
`OBJECT` (nested table) and `VALUE` (any JSON, e.g. arrays). `nyxj gen-struct` writes these tables for you.

## JSON Schema (`nyxj_schema.h`)

```c
nyxj_schema_report rep;
if (!nyxj_schema_validate(schema, doc, &rep))
    for (int i = 0; i < rep.count; i++)
        printf("%s [%s] %s\n", rep.issues[i].path, rep.issues[i].keyword, rep.issues[i].message);
```

## NyxQuery / JSONPath (`nyxj_query.h`)

```c
nyxj_value* titles = nyxj_query(root, "$.store.book[?(@.price < 10 && @.category == 'fiction')].title", &err);
const nyxj_value* first = nyxj_query_first(root, "$..isbn", &err);
nyxj_compiled_query* q = nyxj_query_compile("$..price", &err);   /* Compile once, run many times */
nyxj_query_exec(q, root, callback, user);
nyxj_query_free(q);
```

## Patch and diff (`nyxj_patch.h`)

```c
nyxj_value* ops = nyxj_diff(previous, current);   /* RFC 6902 */
nyxj_patch_apply(&doc, ops, &err);                /* Atomic: doc unchanged on failure */
nyxj_value* delta = nyxj_merge_diff(previous, current);  /* RFC 7386 */
doc = nyxj_merge_patch(doc, delta);
nyxj_pointer_get(doc, "/store/book/0/title");     /* RFC 6901 */
```

## MessagePack (`nyxj_msgpack.h`)

```c
size_t n;
uint8_t* bin = nyxj_msgpack_encode(doc, &n);
nyxj_value* back = nyxj_msgpack_decode(bin, n, &err);
```

## Zero-copy view (`nyxj_view.h`)

```c
nyxj_tok tokens[256];
nyxj_view v = {0};
if (nyxj_view_parse(&v, buf, len, tokens, 256, &err)) {     /* No heap allocation */
    size_t t = nyxj_view_path(&v, "user.name");
    if (nyxj_view_eq(&v, t, "Ada")) { ... }
    size_t n; const char* raw = nyxj_view_raw(&v, t, &n);   /* Points into buf */
}
```

## Streaming (`nyxj_sax.h`)

```c
nyxj_sax_handler h = {0};
h.on_key = my_key; h.on_number = my_number;
nyxj_sax_options o = {.multi_document = true};             /* NDJSON */
nyxj_sax* s = nyxj_sax_create(&h, user, &o);
while ((n = read(fd, chunk, sizeof chunk)) > 0) nyxj_sax_feed(s, chunk, n);
nyxj_sax_finish(s);
nyxj_sax_destroy(s);
```

## Repair (`nyxj_repair.h`)

```c
nyxj_repair_report rep;
char* fixed = nyxj_repair(text, len, NULL, &rep);          /* Valid compact JSON or NULL */
nyxj_result r = nyxj_parse_lenient(text, len, &rep);       /* Strict first, repair as a fallback */
```

## The Guardian (`nyxj_gate.h`)

```c
nyxj_gate_policy p = nyxj_gate_policy_strict();
nyxj_gate_verdict v;
if (!nyxj_gate_check(body, len, &p, &v)) reject(v.reason, v.error.message);
```

## Reliability (`nyxj_reliability.h`)

```c
nyxj_result r = nyxj_parse_verified(buf, len, NULL);      /* Two parses compared, NYXJ_ERR_INTEGRITY on mismatch */
nyxj_tmr* t = nyxj_tmr_create(config);
const nyxj_value* safe = nyxj_tmr_read(t, &repaired);     /* Majority vote, repairs the odd replica */
uint64_t sum = nyxj_checksum(doc);
```

## Thread pool (`nyxj_pool.h`), key-value store (`nyxj_kv.h`), metrics (`nyxj_metrics.h`)

```c
nyxj_pool* pool = nyxj_pool_create(0, 1024);              /* One worker per CPU */
nyxj_pool_submit(pool, task, arg);                         /* false when the queue is full */

nyxj_kv* kv = nyxj_kv_create(0);
nyxj_kv_set(kv, "session:42", value, 30 * 60 * 1000);      /* 30 minute TTL */
nyxj_kv_incr(kv, "hits", 1, &total);
nyxj_kv_save(kv, "store.json", &err);

nyxj_metrics* m = nyxj_metrics_create();
nyxj_metric_inc(nyxj_metrics_counter(m, "orders_total", "Orders", "region=\"eu\""));
char* text = nyxj_metrics_render(m);                       /* Prometheus exposition format */
```

## IPC (`nyxj_ipc.h`)

```c
nyxj_ipc* srv = nyxj_ipc_listen("/tmp/nyxj.sock", &err);
nyxj_ipc* ch = nyxj_ipc_accept(srv, -1, &err);
nyxj_value* msg = nyxj_ipc_recv_json(ch, 1000, &err);

nyxj_shm* ring = nyxj_shm_create("feed", 1 << 20, &err);  /* Producer */
nyxj_shm_write(ring, data, len);
nyxj_shm* in = nyxj_shm_open("feed", &err);               /* Consumer, another process */
char* next = nyxj_shm_read(in, &len);
```

## Web framework and HTTP client (`nyxj_http.h`)

```c
nyxj_app_config cfg = nyxj_app_config_default();
cfg.listen_url = "http://0.0.0.0:8080";
cfg.tls_cert_file = "cert.pem";  cfg.tls_key_file = "key.pem";   /* Optional HTTPS */
nyxj_app* app = nyxj_app_create(&cfg);

nyxj_route_doc doc = {.summary = "Create an order", .tag = "orders",
                      .request_schema = "{\"type\":\"object\",\"required\":[\"sku\"]}"};
nyxj_app_route_ex(app, "POST", "/api/orders", create_order, NULL, NYXJ_ROUTE_ASYNC, &doc);
nyxj_app_use(app, auth_middleware, NULL);
nyxj_app_run(app);
```

Handlers read `nyxj_req_param`, `nyxj_req_query`, `nyxj_req_header` and `nyxj_req_json` (parsed lazily
with the configured limits), and answer with `nyxj_res_json`, `nyxj_res_text`, `nyxj_res_bytes` or
`nyxj_res_error`. Built-in routes: `/health`, `/ready`, `/metrics`, `/api/metrics`, `/openapi.json`, `/docs`.

```c
nyxj_http_response resp;
if (nyxj_http_post_json("https://api.example.com/v1/items", body, 5000, &resp, &err)) {
    nyxj_value* json = nyxj_http_response_json(&resp, &err);
    nyxj_http_response_free(&resp);
}
```

Set `nyxj_http_options.ca_file` to verify server certificates.

## Plugins (`nyxj_plugin.h`) and clustering (`nyxj_cluster.h`)

See `examples/plugin_hello/hello_plugin.c` for a complete plugin and [SERVER.md](SERVER.md) for the
cluster options.
