# NyxJ Engine

> *NyxJ is not just a parser: it is an architecture for silence and speed in a world saturated with data.*

NyxJ is a high-performance JSON engine and micro-service framework written in pure C. It targets embedded
systems, system tools, edge computing and high-concurrency services: one static library, no runtime
dependencies, native builds for Linux, macOS and Windows, and a WebAssembly build of the core.

[![CI](https://github.com/fomadev/nyxj/actions/workflows/ci.yml/badge.svg)](https://github.com/fomadev/nyxj/actions/workflows/ci.yml)
![C11](https://img.shields.io/badge/C-C11-blue) ![License: FPL v1.0](https://img.shields.io/badge/license-FPL%20v1.0-orange)

## Highlights

| Area | What you get |
|---|---|
| **Parsing** | Thread-safe RFC 8259 parser, full escape/UTF-8 validation, SIMD scanning (SSE2 / NEON), line and column on every error |
| **Security** | JSON DoS protection (depth, size, string and element limits), the Guardian sanitizer, Safe-Gate reverse proxy, HTTPS/TLS |
| **Productivity** | Dot-path getters, builder API, JSON-to-struct mapping, C struct code generator, JSON Schema validation, JSONPath queries |
| **Performance** | Zero-copy token view, constant-memory streaming (SAX) parser, arena allocator, worker thread pool, MessagePack |
| **Data tools** | JSON Patch / Merge Patch diffing, heuristic repair of almost-JSON, structural checksums |
| **Framework** | Express-style router with `:params`, middleware, async handlers, OpenAPI + Swagger UI generation, native HTTP client |
| **Operations** | Prometheus metrics, health/readiness probes with scale hints, heartbeat, Eco-Throttle, structured logs |
| **Platform** | Embedded key-value store with TTL and snapshots, plugins (.so/.dll), Unix sockets and shared memory IPC, gossip clustering |
| **Reliability** | Shadow execution (double parse + compare), triple modular redundancy with majority vote |

## Quick start

```bash
git clone https://github.com/fomadev/nyxj.git
cd nyxj
make            # library, CLI, server, example plugin and examples
make test       # unit + integration tests
./build/nyxj-server --port 8000
```

Then open `http://localhost:8000/docs` for the interactive API documentation.

Prerequisites: a C11 compiler (GCC or Clang) and GNU Make. On Windows use MinGW-w64
([w64devkit](https://github.com/skeeto/w64devkit) or MSYS2 UCRT64).

## Use it as a library

```c
#include "nyxj.h"

nyxj_result r = nyxj_parse("{\"user\": {\"name\": \"Ada\", \"langs\": [\"c\", \"go\"]}}");
if (!r.is_valid) {
    printf("error: %s\n", r.error.message);   /* "... at line 1, column 12" */
} else {
    printf("%s\n", nyxj_path_string(r.root, "user.name", "?"));      /* Ada */
    printf("%s\n", nyxj_path_string(r.root, "user.langs[1]", "?"));  /* go  */
    nyxj_free_value(r.root);
}
```

A complete micro-service in a few lines:

```c
#include "nyxj_http.h"

static void get_user(nyxj_request* req, nyxj_response* res) {
    nyxj_value* o = nyxj_new_object();
    nyxj_object_set_string(o, "id", nyxj_req_param(req, "id"));
    nyxj_res_json(res, 200, o);
}

int main(void) {
    nyxj_app* app = nyxj_app_create(NULL);           /* :8000, CORS, metrics, docs, health */
    nyxj_app_get(app, "/api/user/:id", get_user, NULL);
    return nyxj_app_run(app);
}
```

Link with `build/libnyxj.a` and the platform libraries (`-lpthread -lm -ldl -lrt` on Linux,
`-lws2_32 -lpsapi` on Windows). See [docs/API.md](docs/API.md) for every module.

## Command-line toolbox

```bash
nyxj validate data.json                       # syntax check with line/column
nyxj format --indent 4 data.json              # pretty-print (minify for the opposite)
nyxj query '$..book[?(@.price < 10)].title' store.json
nyxj repair broken.json                       # comments, quotes, commas, unclosed brackets...
nyxj schema schema.json data.json             # JSON Schema validation
nyxj diff old.json new.json                   # RFC 6902 JSON Patch (--merge for RFC 7386)
nyxj gen-struct --name Order sample.json      # C structs + mapping tables
nyxj msgpack encode data.json -o data.mp      # binary serialization
nyxj stream huge.ndjson                       # constant-memory statistics
nyxj bench data.json                          # throughput of every engine mode
```

## The server

`nyxj-server` exposes the engine over HTTP(S): `/api/data`, `/api/validate`, `/api/query`, `/api/repair`,
`/api/diff`, `/api/patch`, `/api/msgpack/*`, the key-value store under `/api/kv`, plus `/health`, `/ready`,
`/metrics`, `/openapi.json` and `/docs`. It also runs as a **Safe-Gate** JSON firewall in front of another
service, loads plugins, and joins a gossip cluster.

```bash
nyxj-server --port 8443 --tls-cert cert.pem --tls-key key.pem --threads 8
nyxj-server --gate-upstream http://127.0.0.1:3000          # protect a Node.js/Python back-end
nyxj-server --plugin build/plugins/hello_plugin.so
nyxj-server --cluster-bind udp://0.0.0.0:7946 --seed udp://10.0.0.2:7946
```

All options, endpoints and the JSON configuration file are documented in [docs/SERVER.md](docs/SERVER.md).

## Project structure

```
include/          Public headers (nyxj.h + one header per module)
src/core/         Parser, values, serializer, errors, logging, arena
src/modules/      Schema, query, patch, msgpack, view, SAX, repair, gate, mapping, reliability
src/runtime/      Thread pool, key-value store, metrics, IPC
src/net/          Router/server, HTTP client, Safe-Gate proxy, plugins, cluster
src/server/       nyxj-server
src/cli/          nyxj command-line tool
src/wasm/         WebAssembly bindings
tests/            Test suite and end-to-end smoke test
examples/         API service, HTTP client, struct mapping, streaming, plugin, browser (WASM)
vendor/mongoose/  Networking layer (Mongoose)
```

## Building

| Command | Result |
|---|---|
| `make` | `build/libnyxj.a`, `build/nyxj`, `build/nyxj-server`, plugin and examples |
| `make test` | Runs the test suite |
| `make SAN=1 test` | Tests under AddressSanitizer and UndefinedBehaviorSanitizer |
| `make wasm` | `build/wasm/nyxj.js` + `nyxj.wasm` (requires Emscripten) |
| `make dist PLATFORM=linux-x64` | Release layout in `dist/` |
| `make CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-ar` | Cross-compilation |
| `make install PREFIX=/usr/local` | Installs binaries, library and headers |

## Roadmap

NyxJ 2.0.0 delivers the bulk of the original roadmap. What comes next, from scripting to post-quantum
TLS, is tracked in [ROADMAP.md](ROADMAP.md). Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Contributing

Contributions are welcome: see [CONTRIBUTING.md](CONTRIBUTING.md). Commits follow
[Conventional Commits](https://www.conventionalcommits.org/), which feeds the automated release notes.

## Support

Need a specific integration, a port to a new board or professional support? Open an issue or contact
FomaDev, a Fordima Inc. company, through the GitHub repository.

## License

The NyxJ sources are released under the **Fordima Public License (FPL) v1.0**, see [LICENSE](LICENSE).

- **Free use**: free for individuals, education and companies, including embedding NyxJ in commercial
  products and revenue-generating projects.
- **Contributions**: pull requests are welcome via the official repository.
- **Restrictions**: using this source code to build a similar or competing engine, redistributing it as a
  standalone product, or offering a paid hosted version based on it requires a paid commercial license from
  Fordima Inc. For commercial licensing, please contact the management of Fordima Inc.

Versions of NyxJ before X.Y.Z were released under the MIT license and remain available under those terms.

The networking layer (`vendor/mongoose`) is [Mongoose](https://github.com/cesanta/mongoose) by Cesanta,
dual-licensed under GPL-2.0-only or a commercial license. Binaries that include it (`nyxj-server`, the
HTTP parts of `libnyxj.a`) are subject to those terms. The core engine and the WebAssembly build do not
use Mongoose.

Copyright (c) 2026 Fordima Inc. All rights reserved.

## Contributors

[@fordimalanda](https://github.com/fordimalanda)