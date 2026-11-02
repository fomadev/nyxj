# NyxJ Roadmap

This roadmap tracks where NyxJ comes from and where it is going. Version numbers on future items are
indicative: features ship when they are ready and well tested.

## Delivered in 2.0.0

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

## Next

- **Embedded scripting**: business logic in Lua or QuickJS on top of the C engine (the Nginx/Redis model).
- **WebSocket and Server-Sent Events** streaming endpoints, including JSON Patch deltas pushed to clients.
- **Schema compilation**: pre-compiled validators and mappers for hot schemas, a step towards JIT.
- **Persistent storage** for the key-value store (append-only log) and replication through the cluster.
- **Consensus**: a lightweight Raft layer on top of the gossip membership for leader election.
- **AVX2 and SVE** scanning paths, regular-expression support for `pattern` and NyxQuery `=~`.
- **Embedded SDKs** for ESP32 and STM32 with a reduced, allocation-free profile.
- **Benchmarks** against cJSON, Jansson, yyjson and RapidJSON published with every release.

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
