# NyxJ Roadmap

This roadmap tracks where NyxJ comes from and where it is going. Version numbers on future items are
indicative: features ship when they are ready and well tested.

## Delivered in 2.1.0

| Theme | Feature | Module |
|---|---|---|
| Quality | Structured logging, detailed error codes with line/column | `nyxj_log.h`, `nyxj.h` |
| Quality | Universal result type, memory pool (arena) | `nyxj.h` |
| Productivity | One-line dot-path getters, builder API | `nyxj.h` |
| Productivity | JSON-to-struct mapping and C struct generator ("the Unifier") | `nyxj_map.h` |
| Security | HTTPS/TLS server and client | `nyxj_http.h` |
| Security | JSON Schema validation | `nyxj_schema.h` |
| Security | JSON DoS protection (depth, size, string, element limits) | `nyxj.h` |
| Security | The Guardian sanitizer and the Safe-Gate JSON firewall | `nyxj_gate.h` |
| Connectivity | Native HTTP client | `nyxj_http.h` |
| Scale | Worker thread pool for JSON processing | `nyxj_pool.h` |
| Memory | Embedded key-value store with TTL and snapshots | `nyxj_kv.h` |
| Observability | Prometheus metrics endpoint | `nyxj_metrics.h` |
| Speed | MessagePack binary serialization | `nyxj_msgpack.h` |
| Speed | Zero-copy, non-destructive parsing | `nyxj_view.h` |
| Speed | SIMD scanning (SSE2 / NEON) | core |
| Web | Dynamic routing with `:params`, middleware | `nyxj_http.h` |
| Modularity | Plugins loaded at runtime (`.so` / `.dll`) | `nyxj_plugin.h` |
| DevX | OpenAPI generation and Swagger UI | `nyxj_http.h` |
| Cloud | WebAssembly build of the core | `src/wasm` |
| Big data | Streaming SAX parser with constant memory | `nyxj_sax.h` |
| Cluster | Unix domain sockets and shared memory IPC | `nyxj_ipc.h` |
| Cloud native | Health, readiness, heartbeat, resource monitoring, scale hints | `nyxj_http.h` |
| Critical systems | Shadow execution, triple modular redundancy | `nyxj_reliability.h` |
| Data discovery | NyxQuery (JSONPath with filters) | `nyxj_query.h` |
| Decentralized | Gossip discovery and failure detection (experimental) | `nyxj_cluster.h` |
| Green Ops | Eco-Throttle on CPU temperature and load | `nyxj_http.h` |
| Resilience | Heuristic repair of malformed JSON | `nyxj_repair.h` |
| Bandwidth | Differential state tracking (JSON Patch, Merge Patch) | `nyxj_patch.h` |
| Ergonomics | `NYXJ_ARRAY_FOREACH` / `NYXJ_OBJECT_FOREACH` iteration macros | `nyxj.h` |
| Security | Public strict UTF-8 validator with an SSE2 / NEON ASCII fast path | `nyxj.h` |
| Security | Per-client rate limiting, idle connection timeout, default security headers | `nyxj_http.h` |
| Observability | `X-Request-Id` and W3C `traceparent` propagation, ids in every log line | `nyxj_http.h`, `nyxj_log.h` |
| Durability | Key-value write-ahead log with crash recovery and checkpoints | `nyxj_kv.h` |
| Cluster | Binary (MessagePack) gossip, still reading JSON from older nodes | `nyxj_cluster.h` |
| DevX | CLI `--sort-keys`, `--ascii`, multi-file `validate`, `watch` | `nyxj` |
| DevX | TypeScript definitions for the WebAssembly build | `src/wasm/nyxj.d.ts` |
| Quality | libFuzzer harnesses (8 targets), Valgrind and corpus replay in CI | `tests/fuzz` |
| Quality | Reproducible benchmarks on twitter, canada and citm_catalog | `make bench` |

## Next

- **Embedded scripting**: business logic in Lua or QuickJS on top of the C engine (the Nginx/Redis model).
- **WebSocket and Server-Sent Events** streaming endpoints, including JSON Patch deltas pushed to clients.
- **Schema compilation**: pre-compiled validators and mappers for hot schemas, a step towards JIT.
- **Replication** of the key-value store through the cluster (the write-ahead log is in place).
- **Consensus**: a lightweight Raft layer on top of the gossip membership for leader election.
- **AVX2 and SVE** scanning paths, regular-expression support for `pattern` and NyxQuery `=~`.
- **Embedded SDKs** for ESP32 and STM32 with a reduced, allocation-free profile.
- **Comparative benchmarks** against cJSON, Jansson, yyjson and RapidJSON, published with every release.
  NyxJ's own benchmarks on the standard datasets already run in CI (`make bench`).

## Candidate additions

These ideas came out of a review of the usual blind spots of a hybrid JSON + micro-services engine in
C11. The ones that were missing have been delivered (see the table above); what remains:

- **Chained builder helpers** to build nested documents in one expression.
- **Official language bindings** (Rust, Go, Python) on top of the C API. The C API and the WebAssembly
  exports are already stable within a major version, and the WebAssembly build ships TypeScript
  definitions.
- **Configurable Content-Security-Policy** for services that serve HTML (the default security headers
  leave it out so that the Swagger UI keeps working).
- **Event-driven watch mode** (inotify, kqueue, ReadDirectoryChangesW) instead of polling.
- **Peak-memory figures** in `nyxj bench`, next to the throughput.

## Research and long-term vision

These ideas need research, external dependencies or hardware that NyxJ does not target yet. They stay on
the radar but carry no commitment.

- Inference hooks for small local models (GGML, TinyMaix) and natural-language gateways to the API.
- Post-quantum key exchange in TLS (for example through liboqs).
- JIT compilation of recurring schemas to machine code; branch prediction driven by key frequency.
- Offloading parsing to FPGAs, DPUs or smart NICs.
- Live migration of a running service across CPU architectures.
- Byzantine-tolerant swarms that vote out misbehaving nodes.
- Photonic and other non-silicon computing targets.

Have an idea or want to help? Open an issue to discuss it.
