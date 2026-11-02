# nyxj-server guide

`nyxj-server` exposes the NyxJ engine over HTTP(S). Every option can also be set in a JSON configuration
file (comments and trailing commas allowed).

## Options

| Option | Config key | Description |
|---|---|---|
| `--listen URL` / `--port N` | `listen` | Listen address, default `http://0.0.0.0:8000` |
| `--threads N` | `threads` | Worker threads (0 = one per CPU, -1 = none) |
| `--queue N` | `queue` | Pending async jobs before answering 503 |
| `--max-body BYTES` | `max_body` | Largest accepted body (413 above), default 16 MiB |
| `--max-depth N` | `max_depth` | JSON nesting limit, default 512 |
| `--tls-cert FILE`, `--tls-key FILE` | `tls_cert`, `tls_key` | Enable HTTPS (PEM files) |
| `--log-level LEVEL` | `log_level` | trace, debug, info, warn, error, off |
| `--log-json` | `log_json` | Structured JSON logs |
| `--no-access-log` | `access_log` | Disable per-request log lines |
| `--no-cors` | `cors` | Disable CORS headers |
| `--plugin FILE` | `plugins` (array) | Load plugins |
| `--gate-upstream URL` | `gate_upstream` | Safe-Gate mode |
| `--kv-snapshot FILE` | `kv_snapshot` | Load the key-value store at start, save it on exit |
| `--heartbeat-ms N`, `--heartbeat-url URL` | `heartbeat_ms`, `heartbeat_url` | Resource sampling and webhook |
| `--eco-temp C`, `--eco-load L` | `eco_temp`, `eco_load` | Eco-Throttle thresholds |
| `--cluster-bind URL` | `cluster_bind` | Enable gossip clustering |
| `--cluster-advertise URL`, `--cluster-name NAME` | `cluster_advertise`, `cluster_name` | Node identity |
| `--seed URL` | `seeds` (array) | Gossip seeds |
| `--config FILE` | | Load a configuration file |

```jsonc
// nyxj.json
{
  "listen": "http://0.0.0.0:8080",
  "threads": 8,
  "log_level": "info",
  "plugins": ["build/plugins/hello_plugin.so"],
  "kv_snapshot": "store.json",
  "eco_temp": 80,
}
```

## Endpoints

| Method | Path | Description |
|---|---|---|
| POST | `/api/data` | Parse and echo a document (1.x compatible) |
| POST | `/api/format?indent=N&sort=1` | Pretty-print |
| POST | `/api/validate` | `{"schema": ..., "data": ...}` → `{"valid", "errors"}` (422 when invalid) |
| POST | `/api/query` | `{"query": "$..x", "data": ...}` → `{"count", "matches"}` |
| POST | `/api/repair` | Raw almost-JSON → `{"valid", "repaired", "report", "result"}` |
| POST | `/api/diff?mode=merge` | `{"from", "to"}` → JSON Patch (or Merge Patch) |
| POST | `/api/patch` | `{"document", "patch"}`: array = JSON Patch, object = Merge Patch |
| POST | `/api/msgpack/encode` | JSON → `application/msgpack` |
| POST | `/api/msgpack/decode` | MessagePack → JSON |
| GET | `/api/kv?prefix=` | List keys |
| GET / PUT / DELETE | `/api/kv/:key` | Read, store (`?ttl=ms`) or delete a value |
| POST | `/api/kv/:key/incr?by=N` | Atomic increment |
| GET | `/api/info` | Version, SIMD backend, features |
| GET | `/health` | Liveness |
| GET | `/ready` | Readiness: 503 + `X-NyxJ-Scale-Hint: scale-out` when the queue is saturated |
| GET | `/metrics`, `/api/metrics` | Prometheus metrics |
| GET | `/openapi.json`, `/docs` | OpenAPI document and Swagger UI |
| GET | `/cluster/members` | Cluster view (when clustering is enabled) |

Errors always look like `{"error": "<code>", "message": "...", "status": N}`.

## HTTPS

The built-in TLS stack speaks TLS 1.3 and expects an EC (P-256) certificate:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out key.pem
openssl req -new -x509 -key key.pem -out cert.pem -days 365 -subj "/CN=localhost"
nyxj-server --port 8443 --tls-cert cert.pem --tls-key key.pem
```

## Safe-Gate

```bash
nyxj-server --port 8080 --gate-upstream http://127.0.0.1:3000
```

Requests that match no NyxJ route are checked by the Guardian (size, UTF-8, depth, string and element
limits). Bad payloads are rejected with 400, 413 or 422 and counted in `nyxj_gate_blocked_total{reason}`;
clean ones are forwarded with `X-Forwarded-For` and `X-NyxJ-Gate: passed`.

## Eco-Throttle and heartbeat

Every `--heartbeat-ms` the server samples CPU time, resident memory, load average and CPU temperature
(Linux thermal zones). Above `--eco-temp` or `--eco-load` (load per CPU) the worker concurrency is halved,
and reduced to one worker in the critical zone; it recovers after three calm samples.

## Clustering (experimental)

```bash
nyxj-server --port 8001 --cluster-bind udp://0.0.0.0:7946 --cluster-name alpha
nyxj-server --port 8002 --cluster-bind udp://0.0.0.0:7947 --cluster-name beta --seed udp://127.0.0.1:7946
curl http://localhost:8001/cluster/members
```

Nodes gossip their membership and load once per second over UDP, mark silent peers `suspect` after 5 s
and `dead` after 15 s, and report the least loaded peer for offloading.

## Plugins

A plugin exports `nyxj_plugin_describe` and `nyxj_plugin_init`, receives the host function table and
registers routes. Build the example with `make plugin` and load it with `--plugin`.

## Metrics

`nyxj_http_requests_total{method,route,status}`, `nyxj_http_request_duration_seconds`,
`nyxj_http_requests_in_flight`, `nyxj_http_rejected_total`, `nyxj_json_parse_duration_seconds`,
`nyxj_json_parse_errors_total`, `nyxj_json_parsed_bytes_total`, `nyxj_pool_*`, `nyxj_kv_*`,
`nyxj_process_cpu_seconds_total`, `nyxj_process_resident_memory_bytes`, `nyxj_system_load1`,
`nyxj_cpu_temperature_celsius`, `nyxj_eco_throttle_level`, `nyxj_uptime_seconds`, `nyxj_build_info`,
and `nyxj_gate_*` in Safe-Gate mode.
