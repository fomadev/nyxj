# Changelog

All notable changes to NyxJ are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/). The release pipeline publishes the section matching each tag.

## [2.0.0] - Unreleased

NyxJ 2.0.0 turns the engine into a complete, production-oriented platform: a rewritten thread-safe core,
a dozen data modules, a web framework with operations tooling, and a WebAssembly build.

### Added

#### Core engine
- Thread-safe RFC 8259 parser (no global state) with full string escape decoding, `\u` surrogate pairs,
  strict UTF-8 validation and the exact JSON number grammar.
- Detailed error model: `nyxj_error` with code, byte offset, line, column and message; `nyxj_strerror()`
  and `nyxj_error_name()`. `nyxj_result` now carries the error next to the data.
- JSON Denial-of-Service protection: maximum depth (512 by default), input size, string length and
  elements per container (`nyxj_parse_options`), optional duplicate-key rejection.
- Relaxed mode accepting comments and trailing commas (`NYXJ_PARSE_RELAXED`).
- SIMD-accelerated whitespace and string scanning (SSE2 on x86-64, NEON on AArch64, scalar fallback).
- Arena allocator (memory pool) for fragmentation-free trees released in O(1).
- Builder API (`nyxj_new_*`, `nyxj_object_set*`, `nyxj_array_push/insert/remove`), `nyxj_clone()`,
  `nyxj_equals()`, typed getters and one-line dot-path access (`nyxj_get_path(root, "user.langs[1]")`).
- Linear-time serializer with full escaping, shortest round-trip numbers, pretty-printing, sorted keys
  and optional `\uXXXX` output.
- Structured logging with levels, JSON output, custom sinks and compile-time stripping
  (`NYXJ_LOG_COMPILE_LEVEL`).

#### Data modules
- JSON-to-struct mapping with field descriptors (`nyxj_map.h`), inverse mapping and a C code generator
  (`nyxj gen-struct`).
- JSON Schema validation (types, enums, `$ref`, combinators, `if/then/else`, objects, arrays, numeric and
  string constraints, common formats) with path-aware reports.
- NyxQuery: JSONPath with recursive descent, wildcards, slices, unions and filter expressions.
- Differential state tracking: JSON Pointer, JSON Patch diff/apply (atomic) and JSON Merge Patch.
- MessagePack encoder and decoder.
- Zero-copy token view that indexes the original buffer without copying strings.
- Streaming SAX parser with constant memory, chunked input and multi-document (NDJSON) support.
- Heuristic repair of almost-JSON (comments, quotes, missing/trailing commas, unclosed structures,
  Python/JavaScript literals, sloppy numbers, Markdown fences) and `nyxj_parse_lenient()`.
- The Guardian sanitizer: allocation-free inspection of raw buffers before parsing.
- Fail-safe tools: shadow execution (`nyxj_parse_verified`), triple modular redundancy and checksums.

#### Runtime and networking
- Worker thread pool with bounded queue, backpressure and adjustable concurrency.
- Embedded key-value store with TTL, atomic counters, prefix listing and JSON snapshots.
- Prometheus metrics registry and process/system resource monitoring.
- Unix domain sockets and a lock-free shared-memory ring buffer for inter-process communication.
- Express-style router with `:params` and wildcards, middleware chain, 405 handling, CORS and request
  bodies parsed on the worker pool (`NYXJ_ROUTE_ASYNC`).
- HTTPS/TLS for the server and a native synchronous HTTP/HTTPS client.
- OpenAPI 3 document generated from the routes, served with Swagger UI at `/docs`.
- `/health` and `/ready` probes (readiness returns 503 and `X-NyxJ-Scale-Hint: scale-out` under load),
  heartbeat sampling and optional heartbeat webhook.
- Eco-Throttle: worker concurrency reduced when CPU temperature or load crosses a threshold.
- Safe-Gate reverse proxy that sanitizes requests before forwarding them to an upstream service.
- Plugin system loading `.so`/`.dll` modules through a versioned host function table.
- Experimental gossip clustering over UDP with peer discovery, failure detection and load sharing hints.

#### Tooling
- `nyxj` CLI: validate, format, minify, get, query, repair, schema, diff, patch, msgpack, gen-struct,
  stream, tokens, gate and bench.
- `nyxj-server` options for every feature and a JSON configuration file.
- WebAssembly build of the core (`make wasm`) with a browser demo and Node.js smoke test.
- Test suite (500+ checks) covering every module plus a live client/server integration test and an
  end-to-end smoke script; CI on Linux x64/ARM64, macOS, Windows, sanitizers and WebAssembly.
- Release pipeline: a `v*` tag builds and tests Linux x64/ARM64, macOS, Windows and WebAssembly, then
  publishes a GitHub release with generated notes (changelog highlights, grouped commits, pull requests,
  contributors), archives, the OpenAPI document, SHA-256 checksums and build provenance. A manual run
  rehearses the release without publishing.
- Examples: struct mapping, streaming sensor feed, plugin, browser demo.
- `DOCUMENTATION.md`: a complete, beginner-friendly guide (36 chapters and appendices) that teaches
  NyxJ from JSON basics and installation to production services, with compiled and tested examples;
  API and server quick references in `docs/`, and the ROADMAP.

### Changed
- The build now produces `build/libnyxj.a`, `build/nyxj`, `build/nyxj-server`, the example plugin and the
  examples from a single cross-platform Makefile.
- `nyxj_parse()` returns `root == NULL` on error instead of a partial tree.
- Error responses use one shape: `{"error": "<code>", "message": "...", "status": N}`.
- Arrays now track their capacity: build values with the builder API rather than raw `malloc`.
- The API and client examples use the router and the HTTP client.
- The README is a short overview that points to the complete guide.

### Fixed
- String escapes are decoded when parsing and escaped when serializing (they were copied verbatim).
- Numbers with exponents and invalid forms such as `-` or `01` are handled per the JSON grammar.
- Missing allocation checks and memory leaks on error paths in the parser.
- The client example referenced an undefined field (`msg.text`).

### Security
- Nesting depth is limited by default, closing the stack exhaustion vector of deeply nested payloads.
- Response headers are sanitized against CR/LF injection; request bodies above `max_body_size` get 413.

### Removed
- `src/server.c` (replaced by `src/server/main.c`) and the per-example Makefiles.

## [1.0.0] - 2026-04-23

### Added
- Recursive descent JSON parser and serializer.
- Getter API for objects.
- Mongoose-powered REST micro-service (`POST /api/data`).
- API and client examples, multi-platform build workflow.

[2.0.0]: https://github.com/fomadev/nyxj/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/fomadev/nyxj/releases/tag/v1.0.0
