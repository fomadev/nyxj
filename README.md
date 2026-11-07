# NyxJ Engine

> *NyxJ is not just a parser: it is an architecture for silence and speed in a world saturated with data.*

[![CI](https://github.com/fomadev/nyxj/actions/workflows/ci.yml/badge.svg)](https://github.com/fomadev/nyxj/actions/workflows/ci.yml)
![C11](https://img.shields.io/badge/C-C11-blue) ![License: FPL v1.0](https://img.shields.io/badge/license-FPL%20v1.0-orange)

NyxJ is a high-performance JSON engine and micro-service framework written in pure C11. It ships as one
static library with no runtime dependencies, builds natively on Linux, macOS and Windows, and compiles its
core to WebAssembly.

## Features

- **Fast, safe parsing**: thread-safe RFC 8259 parser, SIMD scanning (SSE2 / NEON), line and column on
  every error, built-in JSON denial-of-service limits.
- **Data toolkit**: dot-path getters, builder API, JSON-to-struct mapping, JSON Schema, JSONPath queries,
  JSON Patch, MessagePack and repair of almost-JSON.
- **Big data**: zero-copy token view, constant-memory streaming parser and arena allocator.
- **Web framework**: router with `:params`, middleware, async handlers, HTTPS, OpenAPI with Swagger UI,
  and an HTTP client.
- **Operations**: Prometheus metrics, health and readiness probes, embedded key-value store, plugins, IPC
  and gossip clustering.

## Quick start

```bash
git clone https://github.com/fomadev/nyxj.git
cd nyxj
make
make test
./build/nyxj-server --port 8000
```

Then open `http://localhost:8000/docs`. You need a C11 compiler (GCC or Clang) and GNU Make; on Windows,
use MinGW-w64 ([w64devkit](https://github.com/skeeto/w64devkit) or MSYS2 UCRT64).

## Example

```c
#include "nyxj.h"
#include <stdio.h>

int main(void) {
    nyxj_result r = nyxj_parse("{\"user\": {\"name\": \"Ada\", \"langs\": [\"c\", \"go\"]}}");
    if (!r.is_valid) {
        printf("error: %s\n", r.error.message);
        return 1;
    }
    printf("%s\n", nyxj_path_string(r.root, "user.langs[1]", "?"));  /* go */
    nyxj_free_value(r.root);
    return 0;
}
```

```bash
nyxj validate data.json                              # syntax check with line and column
nyxj query '$..book[?(@.price < 10)].title' store.json
nyxj repair broken.json                              # fix comments, quotes, commas...
```

## Documentation

- **[DOCUMENTATION.md](DOCUMENTATION.md)**: the complete guide, from your first build to production,
  written so that a beginner can learn NyxJ from start to finish.
- [docs/API.md](docs/API.md) and [docs/SERVER.md](docs/SERVER.md): quick references for the C API and
  `nyxj-server`.
- [CHANGELOG.md](CHANGELOG.md) and [ROADMAP.md](ROADMAP.md): what changed and what comes next.

## Contributing

Contributions are welcome: read [CONTRIBUTING.md](CONTRIBUTING.md). Commits follow
[Conventional Commits](https://www.conventionalcommits.org/), which feed the automated release notes. For
a specific integration or professional support, open an issue or contact FomaDev, a Fordima Inc. company.

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
