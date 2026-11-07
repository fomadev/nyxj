# The NyxJ Book

**The complete guide to NyxJ 2.0.0, from your first build to production.**

This document teaches NyxJ from the beginning to the end. It assumes no previous knowledge of NyxJ and
explains every idea before using it, so a beginner can read it in order and come out understanding the
whole engine. Experienced developers can jump straight to the chapter they need: every chapter starts
with what it covers and ends with a short recap.

## Contents

**[Part I. Getting Started](#part-i-getting-started)**

- [Chapter 0. Introduction](#chapter-0-introduction)
- [Chapter 1. JSON in Five Minutes](#chapter-1-json-in-five-minutes)
- [Chapter 2. Installing NyxJ](#chapter-2-installing-nyxj)
- [Chapter 3. A Tour of the Project](#chapter-3-a-tour-of-the-project)
- [Chapter 4. The Command-Line Toolbox](#chapter-4-the-command-line-toolbox)
- [Chapter 5. Your First Program](#chapter-5-your-first-program)

**[Part II. The Core Engine](#part-ii-the-core-engine)**

- [Chapter 6. Values and Memory](#chapter-6-values-and-memory)
- [Chapter 7. Parsing in Depth](#chapter-7-parsing-in-depth)
- [Chapter 8. Reading Data](#chapter-8-reading-data)
- [Chapter 9. Building and Writing JSON](#chapter-9-building-and-writing-json)
- [Chapter 10. The Arena Allocator](#chapter-10-the-arena-allocator)
- [Chapter 11. Logging](#chapter-11-logging)

**[Part III. Data Modules](#part-iii-data-modules)**

- [Chapter 12. Mapping JSON to C Structs](#chapter-12-mapping-json-to-c-structs)
- [Chapter 13. Validating with JSON Schema](#chapter-13-validating-with-json-schema)
- [Chapter 14. Querying with NyxQuery](#chapter-14-querying-with-nyxquery)
- [Chapter 15. Tracking Changes: Pointer, Patch and Merge Patch](#chapter-15-tracking-changes-pointer-patch-and-merge-patch)
- [Chapter 16. MessagePack](#chapter-16-messagepack)
- [Chapter 17. The Zero-Copy View](#chapter-17-the-zero-copy-view)
- [Chapter 18. Streaming Huge Data: the SAX Parser](#chapter-18-streaming-huge-data-the-sax-parser)
- [Chapter 19. Repairing Broken JSON](#chapter-19-repairing-broken-json)
- [Chapter 20. Security: Limits and the Guardian](#chapter-20-security-limits-and-the-guardian)
- [Chapter 21. Reliability for Critical Systems](#chapter-21-reliability-for-critical-systems)

**[Part IV. Runtime Services](#part-iv-runtime-services)**

- [Chapter 22. Concurrency: the Thread Pool](#chapter-22-concurrency-the-thread-pool)
- [Chapter 23. The Embedded Key-Value Store](#chapter-23-the-embedded-key-value-store)
- [Chapter 24. Metrics and System Monitoring](#chapter-24-metrics-and-system-monitoring)
- [Chapter 25. Inter-Process Communication](#chapter-25-inter-process-communication)

**[Part V. Web and Network](#part-v-web-and-network)**

- [Chapter 26. Building a Web Service](#chapter-26-building-a-web-service)
- [Chapter 27. Calling Other Services: the HTTP Client](#chapter-27-calling-other-services-the-http-client)
- [Chapter 28. Running in Production](#chapter-28-running-in-production)
- [Chapter 29. Extending NyxJ with Plugins](#chapter-29-extending-nyxj-with-plugins)
- [Chapter 30. Clustering (Experimental)](#chapter-30-clustering-experimental)
- [Chapter 31. nyxj-server: the Ready-Made Server](#chapter-31-nyxj-server-the-ready-made-server)

**[Part VI. Going Further](#part-vi-going-further)**

- [Chapter 32. NyxJ in the Browser: WebAssembly](#chapter-32-nyxj-in-the-browser-webassembly)
- [Chapter 33. Building, Testing and Releasing](#chapter-33-building-testing-and-releasing)
- [Chapter 34. Best Practices and Troubleshooting](#chapter-34-best-practices-and-troubleshooting)
- [Chapter 35. Where to Go Next](#chapter-35-where-to-go-next)

**Appendices**

- [Appendix A. Error Codes](#appendix-a-error-codes)
- [Appendix B. API Index](#appendix-b-api-index)
- [Appendix C. Glossary](#appendix-c-glossary)

---

# Part I. Getting Started

## Chapter 0. Introduction

### What is NyxJ?

NyxJ is a **JSON engine** and a **micro-service framework** written in the C programming language.

- As a **JSON engine**, it reads JSON text (this is called *parsing*), lets you explore and modify the
  data, and writes it back as text (*serializing*). Around this core it adds tools to validate, query,
  compare, repair, compress and stream JSON.
- As a **micro-service framework**, it lets you build a web server (an HTTP API) in a few lines of C, with
  routing, security limits, metrics, health checks and automatic documentation.

Everything comes in one static library (`libnyxj.a`), two ready-made programs (the `nyxj` command-line
tool and the `nyxj-server` HTTP server) and a WebAssembly build that runs in browsers and Node.js.

### Why does NyxJ exist?

JSON is everywhere: web APIs, configuration files, logs, IoT sensors, databases. Most programs spend a
surprising amount of time and memory reading and writing it, and badly formed or hostile JSON is a
classic source of crashes and security holes. NyxJ was designed around three goals:

1. **Speed and frugality**: SIMD instructions, zero-copy reading, memory pools and a streaming parser
   whose memory use never grows, so NyxJ runs on a small board as well as on a big server.
2. **Safety**: strict validation of every byte, precise error messages (line and column), and limits that
   stop malicious documents before they exhaust memory or the stack.
3. **Completeness**: the tools you usually assemble from five different libraries (schema validation,
   JSONPath, patches, MessagePack, an HTTP server, metrics...) are already there and work together.

### Who is this guide for?

- **Beginners** who know a little C (variables, functions, pointers, `struct`) and want to learn how to
  work with JSON properly. Chapter 1 even explains JSON itself.
- **Application developers** who want to build a fast HTTP API or command-line tool in C.
- **Embedded and systems developers** who need predictable memory use and no dependencies.
- **Operators** who only want to run `nyxj-server` or use the `nyxj` tool: Chapters 2, 4 and 31 are for
  you, no C required.

### What you will learn

| Part | Chapters | You will learn to... |
|---|---|---|
| I. Getting started | 0 to 5 | Understand JSON, install NyxJ, use the command line, write your first program |
| II. The core engine | 6 to 11 | Parse, explore, build and write JSON safely, manage memory, log |
| III. Data modules | 12 to 21 | Map JSON to structs, validate, query, diff, compress, stream, repair, secure |
| IV. Runtime services | 22 to 25 | Use threads, the key-value store, metrics and inter-process communication |
| V. Web and network | 26 to 31 | Build web services, call other services, run in production, extend and cluster |
| VI. Going further | 32 to 35 | Use WebAssembly, build and test NyxJ, apply best practices, contribute |

### How to read this guide

- Read Part I in order. After it you can already use NyxJ.
- Part II is the foundation of everything else: read it before the later parts.
- Parts III to V can be read in any order, depending on what you need.
- Every complete program in this guide compiles as shown. Type them in (or copy them) and run them:
  experimenting is the fastest way to learn.

### Conventions

- `Code like this` is a file name, a command, a function or a value.
- Shell commands are written for a POSIX shell (Linux, macOS, Git Bash or MSYS2 on Windows).
- `$NYXJ` stands for the folder where you cloned NyxJ, for example `/home/ada/nyxj`.
- Notes that save you from a classic mistake are marked **Pitfall**. Notes that explain a design choice
  are marked **Why**.

### Recap

- NyxJ is a JSON engine plus a micro-service framework, in pure C, with no runtime dependencies.
- It focuses on speed, safety and completeness.
- Part I gets you running; Part II is the foundation; the rest can be read on demand.

---

## Chapter 1. JSON in Five Minutes

If you already know JSON well, skip to Chapter 2.

### What JSON is

JSON (JavaScript Object Notation) is a text format for structured data. It is defined by a standard
called **RFC 8259**. Here is a JSON document describing a user:

```json
{
  "name": "Ada Lovelace",
  "age": 36,
  "admin": true,
  "email": null,
  "languages": ["en", "fr"],
  "address": {
    "city": "London",
    "zip": "W1"
  }
}
```

### The six types

Every JSON value has one of six types:

| Type | Examples | Notes |
|---|---|---|
| **null** | `null` | "No value" |
| **boolean** | `true`, `false` | Always lowercase |
| **number** | `36`, `-2.5`, `1e9` | No leading `+`, no leading zeros (`007`), no `NaN` or `Infinity` |
| **string** | `"hello"`, `"café"` | Always double quotes; special characters are escaped with `\` |
| **array** | `[1, 2, 3]`, `[]` | An ordered list of values, of any types |
| **object** | `{"a": 1}`, `{}` | A set of `"key": value` members; keys are always strings |

Arrays and objects are **containers**: they hold other values, which can themselves be containers. A JSON
document is therefore a **tree**. In the example above, the root is an object; its member `address` is
another object; `languages` is an array of two strings.

### The rules people break

Strict JSON does **not** allow:

- comments (`// ...` or `/* ... */`);
- single quotes (`'text'`);
- unquoted keys (`{name: "Ada"}`);
- trailing commas (`[1, 2, ]`);
- control characters (like a raw new line) inside strings: they must be escaped as `\n`.

Many tools produce such "almost-JSON" anyway (config files written by hand, JavaScript objects, output of
AI models). NyxJ rejects them by default, but can accept comments and trailing commas on demand
(Chapter 7) or repair the rest (Chapter 19).

### Why "parsing" matters

Your program receives JSON as **text**: a sequence of bytes. To use it, it must be turned into a data
structure in memory (the tree). This step, **parsing**, must check every byte: a missing quote, an invalid
UTF-8 character or a document nested 100 000 levels deep must be detected and reported, not crash the
program. The opposite step, turning the tree back into text, is **serialization**. NyxJ does both.

### Recap

- JSON has six types: null, boolean, number, string, array and object.
- A JSON document is a tree of values.
- Strict JSON forbids comments, single quotes, unquoted keys and trailing commas.
- Parsing turns text into a tree; serializing turns a tree into text.

---

## Chapter 2. Installing NyxJ

### What you need

| Platform | Compiler and tools |
|---|---|
| **Linux** | GCC or Clang, GNU Make, Git. On Debian or Ubuntu: `sudo apt install build-essential git` |
| **macOS** | Xcode command-line tools: `xcode-select --install` (provides Clang, Make and Git) |
| **Windows** | MinGW-w64: [w64devkit](https://github.com/skeeto/w64devkit) (unzip and run `w64devkit.exe`) or [MSYS2](https://www.msys2.org/) with the UCRT64 environment (`pacman -S mingw-w64-ucrt-x86_64-gcc make git`) |

NyxJ needs nothing else: no package manager, no external library. The networking layer (Mongoose) is
included in the repository under `vendor/`.

### Get the source code

```bash
git clone https://github.com/fomadev/nyxj.git
cd nyxj
```

### Build

```bash
make
```

This compiles everything into the `build/` folder:

| File | What it is |
|---|---|
| `build/libnyxj.a` | The NyxJ library, to link into your own programs |
| `build/nyxj` (`nyxj.exe` on Windows) | The command-line toolbox (Chapter 4) |
| `build/nyxj-server` | The ready-made HTTP server (Chapter 31) |
| `build/plugins/hello_plugin.so` (`.dll` on Windows) | An example plugin (Chapter 29) |
| `build/examples/*` | Example programs |

The build uses `-O2` optimizations by default. Use `make -j8` to compile on 8 cores at once.

### Run the tests

```bash
make test
```

The test suite checks the parser, every module, the runtime services and a live HTTP server. A line
such as `All tests passed` at the end means your build is healthy.

### Check the installation

```bash
./build/nyxj version
```

```text
nyxj 2.0.0
simd: sse2
features: core,schema,query,patch,msgpack,view,sax,repair,gate,reliability,arena,threads,simd-sse2,http,router,client,tls,metrics,kv,plugins,ipc,cluster
```

`simd` tells you which vector instructions NyxJ uses on your CPU (`sse2` on x86-64, `neon` on ARM64,
`scalar` otherwise). `features` lists the modules compiled in.

### Install system-wide (optional)

```bash
sudo make install                     # into /usr/local
make install PREFIX=$HOME/.local      # or into your home folder, no sudo
```

This copies `nyxj` and `nyxj-server` to `PREFIX/bin`, `libnyxj.a` to `PREFIX/lib` and the headers to
`PREFIX/include/nyxj`. You can also download prebuilt packages for Linux, macOS, Windows and WebAssembly
from the [GitHub releases](https://github.com/fomadev/nyxj/releases) page once a version is published.

### Troubleshooting

| Symptom | Fix |
|---|---|
| `make: command not found` | Install Make (see the table above). On Windows, run the commands inside w64devkit or the MSYS2 UCRT64 shell |
| `gcc: command not found` | Install a compiler, or use Clang: `make CC=clang` |
| Errors about `pthread` or `ws2_32` | You are mixing toolchains (for example MSVC). Use GCC or Clang from the table above |
| A test fails | Run `make clean && make test`; if it still fails, open an issue with the output |

### Recap

- `git clone`, then `make`, then `make test`.
- The build produces a library, a CLI, a server, a plugin and examples in `build/`.
- `nyxj version` shows the version, the SIMD backend and the compiled features.

---

## Chapter 3. A Tour of the Project

Knowing where things live makes everything else easier.

```text
nyxj/
├── include/          Public headers: what you #include in your programs
├── src/
│   ├── common/       Small internal helpers (threads, growable buffers)
│   ├── core/         Parser, values, serializer, errors, logging, arena
│   ├── modules/      Schema, query, patch, msgpack, view, SAX, repair, gate, mapping, reliability
│   ├── runtime/      Thread pool, key-value store, metrics, IPC
│   ├── net/          Web framework, HTTP client, Safe-Gate proxy, plugins, cluster
│   ├── cli/          The nyxj command-line tool
│   ├── server/       The nyxj-server program
│   └── wasm/         WebAssembly bindings
├── tests/            Test suite and end-to-end smoke test
├── examples/         Example programs (API service, client, mapping, streaming, plugin, browser)
├── vendor/mongoose/  The networking layer (Mongoose)
├── docs/             Quick references (API.md, SERVER.md)
└── Makefile          The build
```

### The layers

NyxJ is organized in layers. Each layer only uses the layers below it:

```text
   ┌──────────────────────────────────────────────────────────────┐
   │  net:      web framework · HTTP client · proxy · plugins     │  needs Mongoose
   ├──────────────────────────────────────────────────────────────┤
   │  runtime:  thread pool · key-value store · metrics · IPC     │  needs threads
   ├──────────────────────────────────────────────────────────────┤
   │  modules:  schema · query · patch · msgpack · view · SAX ... │
   ├──────────────────────────────────────────────────────────────┤
   │  core:     parser · values · serializer · errors · arena     │  pure C, portable
   └──────────────────────────────────────────────────────────────┘
```

**Why**: the core and the modules use nothing but standard C. That is what lets them compile to
WebAssembly or run on a microcontroller, while the upper layers add threads and networking on systems
that have them.

### The headers

You only include the headers you need. Each one documents its functions in comments.

| Header | Contents | Chapter |
|---|---|---|
| `nyxj.h` | Values, parsing, serialization, errors, paths, arena | 5 to 10 |
| `nyxj_log.h` | Structured logging | 11 |
| `nyxj_map.h` | JSON to C struct mapping, code generator | 12 |
| `nyxj_schema.h` | JSON Schema validation | 13 |
| `nyxj_query.h` | NyxQuery (JSONPath) | 14 |
| `nyxj_patch.h` | JSON Pointer, JSON Patch, Merge Patch | 15 |
| `nyxj_msgpack.h` | MessagePack | 16 |
| `nyxj_view.h` | Zero-copy view | 17 |
| `nyxj_sax.h` | Streaming parser | 18 |
| `nyxj_repair.h` | Repair of almost-JSON | 19 |
| `nyxj_gate.h` | The Guardian sanitizer | 20 |
| `nyxj_reliability.h` | Checksums, shadow execution, triple redundancy | 21 |
| `nyxj_pool.h` | Thread pool | 22 |
| `nyxj_kv.h` | Key-value store | 23 |
| `nyxj_metrics.h` | Metrics and system monitoring | 24 |
| `nyxj_ipc.h` | Unix sockets and shared memory | 25 |
| `nyxj_http.h` | Web framework and HTTP client | 26 to 28 |
| `nyxj_plugin.h` | Plugin interface | 29 |
| `nyxj_cluster.h` | Gossip clustering | 30 |

### Naming conventions

- Every public function, type and macro starts with `nyxj_` or `NYXJ_`.
- Functions are named `nyxj_<module>_<action>`: `nyxj_kv_set`, `nyxj_query_compile`, `nyxj_sax_feed`.
- Error codes are `NYXJ_ERR_*`, flags are `NYXJ_PARSE_*`, `NYXJ_FIX_*`, and so on.

### Recap

- `include/` holds the headers you use; `src/` is organized in four layers.
- The core and modules are pure portable C; runtime adds threads; net adds networking.
- One header per module, and every name starts with `nyxj_`.

---

## Chapter 4. The Command-Line Toolbox

The `nyxj` program puts the whole engine at your fingertips without writing any C. It is also the
quickest way to discover what NyxJ can do. To follow along, add the build folder to your `PATH` (or
type `./build/nyxj` instead of `nyxj`):

```bash
export PATH="$PWD/build:$PATH"
```

### General usage

```text
nyxj <command> [options] [file]
```

- When the file is omitted (or is `-`), `nyxj` reads from the standard input, so it works in pipes:
  `curl -s https://api.example.com/items | nyxj format`.
- `-o FILE` writes the result to a file instead of the screen.
- `--indent N` sets the indentation of pretty-printed output (2 by default).
- The exit code is `0` on success and `1` on failure, which makes `nyxj` easy to use in scripts.
- `nyxj help` lists every command.

Let us create a few sample files to play with:

```bash
printf '{"store":{"book":[{"title":"Dune","price":8.99,"category":"fiction"},{"title":"SICP","price":45,"category":"cs"},{"title":"Neuromancer","price":7.5,"category":"fiction"}]}}' > store.json
printf '{\n  "name": "Ada",\n  "age": 36,\n  "langs": ["c", "go"\n}\n' > broken.json
```

### validate: is this valid JSON?

```bash
nyxj validate store.json
nyxj validate broken.json
```

```text
store.json: valid JSON (object, 1 top-level members)
nyxj: broken.json: unexpected character '}' at line 5, column 1 (expected ',' or ']') [UNEXPECTED_CHAR]
```

The error tells you **what** is wrong, **where** (line 5, column 1) and **what was expected** (the array
`langs` was never closed). The code in brackets (`UNEXPECTED_CHAR`) is the machine-readable error name.

### format and minify: change the layout

```bash
nyxj format store.json              # pretty-print with 2 spaces
nyxj format --indent 4 store.json   # with 4 spaces
nyxj minify store.json              # remove every useless space
```

```text
{
  "store": {
    "book": [
      {
        "title": "Dune",
        "price": 8.99,
...
```

### get: extract one value

`get` takes a **dot path** (`store.book[1].title`) or a **JSON Pointer** (`/store/book/0/price`):

```bash
nyxj get 'store.book[1].title' store.json      # SICP
nyxj get /store/book/0/price store.json        # 8.99
echo '{"user":{"name":"Ada"}}' | nyxj get user.name   # Ada
```

**Pitfall**: in Git Bash on Windows, arguments starting with `/` are converted to Windows paths. Prefix
the command with `MSYS_NO_PATHCONV=1` when you use JSON Pointers there.

### query: search with JSONPath

```bash
nyxj query '$..book[?(@.price < 10)].title' store.json
```

```text
[
  "Dune",
  "Neuromancer"
]
```

This reads: "anywhere in the document (`..`), take the `book` array, keep the items whose price is below
10 (`?(...)`), and return their `title`". Chapter 14 teaches the full syntax.

### repair: fix almost-JSON

```bash
printf "{name: 'Ada', age: 36, // comment\n langs: ['c', 'go',],}" > almost.json
nyxj repair almost.json
```

```text
nyxj: 9 fix(es): comment quotes unquoted_key trailing_comma
{
  "name": "Ada",
  "age": 36,
  "langs": [
    "c",
    "go"
  ]
}
```

The first line (on the error stream) summarizes the corrections; the repaired JSON goes to the output.

### schema: validate against a JSON Schema

```bash
printf '{"type":"object","required":["name","age"],"properties":{"name":{"type":"string"},"age":{"type":"integer","minimum":0}}}' > user.schema.json
printf '{"name":"Ada","age":-3}' > user.json
nyxj schema user.schema.json user.json
```

```text
  $.age: [minimum] -3 is less than the minimum 0
```

### diff and patch: compare and update documents

```bash
printf '{"name":"Ada","age":36,"langs":["c"]}' > v1.json
printf '{"name":"Ada","age":37,"langs":["c","go"],"city":"London"}' > v2.json
nyxj diff v1.json v2.json            # RFC 6902 JSON Patch
nyxj diff --merge v1.json v2.json    # RFC 7386 Merge Patch
nyxj diff v1.json v2.json -o changes.json
nyxj patch v1.json changes.json      # applies the patch: gives v2 back
```

```text
[
  { "op": "replace", "path": "/age", "value": 37 },
  { "op": "add", "path": "/langs/1", "value": "go" },
  { "op": "add", "path": "/city", "value": "London" }
]
```

(The real output is pretty-printed over more lines.) Chapter 15 explains both patch formats.

### msgpack: binary serialization

```bash
nyxj msgpack encode store.json -o store.mp     # nyxj: 172 bytes of JSON -> 136 bytes of MessagePack (79.1%)
nyxj msgpack decode store.mp
```

### gen-struct: generate C code from a sample

```bash
printf '{"id":1042,"customer":"Ada","total":99.9,"paid":true}' > order.json
nyxj gen-struct --name Order order.json
```

```c
typedef struct {
    int id;
    char* customer;
    double total;
    bool paid;
} Order;

static const nyxj_field Order_fields[] = {
    NYXJ_FIELD_INT(Order, id, "id", true),
    NYXJ_FIELD_STRING(Order, customer, "customer", true),
    NYXJ_FIELD_DOUBLE(Order, total, "total", true),
    NYXJ_FIELD_BOOL(Order, paid, "paid", true),
};
```

Chapter 12 shows how to use this generated code.

### stream, tokens, gate and bench

```bash
nyxj stream feed.ndjson          # statistics over a huge file or NDJSON stream, constant memory
echo '{"a":[1,"x"]}' | nyxj tokens   # shows the zero-copy token view
nyxj gate --strict body.json     # runs the Guardian security checks
nyxj bench store.json            # measures the speed of every engine mode on your machine
```

`nyxj stream` prints counts (documents, objects, keys, numbers, maximum depth...) and the parser's memory
footprint, which stays the same whatever the file size. `nyxj tokens` output looks like this:

```text
#0    object  size=1    [0..13) {"a":[1,"x"]}
#1    string  size=0    [2..3) a
#2    array   size=2    [5..12) [1,"x"]
#3    number  size=0    [6..7) 1
#4    string  size=0    [9..10) x
5 tokens, zero string copies
```

### Recap

| Command | Purpose |
|---|---|
| `validate`, `format`, `minify` | Check and lay out JSON |
| `get`, `query` | Extract values with a path, a pointer or JSONPath |
| `repair` | Fix almost-JSON |
| `schema` | Validate against a JSON Schema |
| `diff`, `patch` | Compute and apply changes |
| `msgpack` | Convert to and from MessagePack |
| `gen-struct` | Generate C structs and mapping tables |
| `stream`, `tokens`, `gate`, `bench` | Stream, inspect, secure and benchmark |

---

## Chapter 5. Your First Program

Time to write C. This chapter walks through a complete program line by line, then shows how to compile
it on every platform.

### The program

Create a file `hello.c`:

```c
#include "nyxj.h"
#include <stdio.h>

int main(void) {
    const char* text = "{\"name\": \"Ada\", \"age\": 36, \"langs\": [\"c\", \"go\"]}";

    /* 1. Parse the text into a tree. */
    nyxj_result r = nyxj_parse(text);

    /* 2. Always check the result before using it. */
    if (!r.is_valid) {
        printf("Invalid JSON: %s\n", r.error.message);
        return 1;
    }

    /* 3. Read values. */
    const char* name = nyxj_get_string(r.root, "name");
    double age = nyxj_get_number(r.root, "age");
    const char* second = nyxj_path_string(r.root, "langs[1]", "none");
    printf("%s is %.0f years old and also writes %s.\n", name, age, second);

    /* 4. Free the tree when you are done. */
    nyxj_free_value(r.root);
    return 0;
}
```

### Line by line

1. `#include "nyxj.h"` gives access to the core API.
2. `nyxj_parse(text)` reads the whole string and returns a `nyxj_result`. Think of it as a box holding
   either the data or an error:
   - `r.is_valid` is `true` when parsing succeeded;
   - `r.root` points to the root of the tree (here, the object);
   - `r.error` describes the problem when parsing failed.
3. `nyxj_get_string`, `nyxj_get_number` and `nyxj_path_string` read members. The path `"langs[1]"`
   means "the member `langs`, then the item at index 1" (indexes start at 0). The last argument of
   `nyxj_path_string` is a fallback returned when the path does not exist.
4. `nyxj_free_value(r.root)` releases the memory used by the tree. Every tree returned by a parse
   function must be freed exactly once.

### Compile and run

The program needs the headers (`-I`) and the library. The library must come **after** your source file,
followed by the system libraries NyxJ uses:

```bash
# Linux
gcc -std=c11 -I$NYXJ/include hello.c $NYXJ/build/libnyxj.a -lpthread -lm -ldl -lrt -o hello

# macOS
clang -std=c11 -I$NYXJ/include hello.c $NYXJ/build/libnyxj.a -lpthread -lm -o hello

# Windows (MinGW-w64)
gcc -std=c11 -I$NYXJ/include hello.c $NYXJ/build/libnyxj.a -lws2_32 -lpsapi -o hello.exe
```

If you ran `make install`, use the installed copy instead: `gcc -std=c11 -I/usr/local/include/nyxj
hello.c -lnyxj -lpthread -lm -ldl -lrt -o hello`.

```bash
./hello
```

```text
Ada is 36 years old and also writes go.
```

### See an error

Change the text to something invalid, for example remove the closing `}`:

```c
const char* text = "{\"name\": \"Ada\", \"age\": 36";
```

Recompile and run:

```text
Invalid JSON: unexpected end of input at line 1, column 26 (expected ',' or '}')
```

NyxJ never crashes on bad input: it tells you exactly what went wrong.

### Using a Makefile for your project

```makefile
NYXJ ?= ../nyxj
CFLAGS = -std=c11 -O2 -Wall -I$(NYXJ)/include
LIBS = $(NYXJ)/build/libnyxj.a -lpthread -lm -ldl -lrt   # see above for macOS and Windows

hello: hello.c
	$(CC) $(CFLAGS) $< $(LIBS) -o $@
```

### Recap

- `nyxj_parse` returns a `nyxj_result`: check `is_valid`, use `root`, read `error` on failure.
- Read values with getters (`nyxj_get_*`) or paths (`nyxj_path_*`).
- Free every parsed tree with `nyxj_free_value`.
- Link with `libnyxj.a` placed after your source file, plus the system libraries of your platform.

---

# Part II. The Core Engine

## Chapter 6. Values and Memory

Every JSON value in memory is a `nyxj_value`. Understanding this structure, and who is responsible for
freeing what, is the most important lesson of this guide.

### The nyxj_value structure

Here is a simplified view of the definition in `nyxj.h`:

```c
struct nyxj_value {
    nyxj_type type;                 /* NYXJ_NULL, NYXJ_BOOL, NYXJ_NUMBER, NYXJ_STRING, NYXJ_ARRAY, NYXJ_OBJECT */
    union {
        bool boolean;               /* when type == NYXJ_BOOL */
        double number;              /* when type == NYXJ_NUMBER */
        char* string;               /* when type == NYXJ_STRING (UTF-8, NUL-terminated) */
        struct {
            nyxj_value** items;     /* when type == NYXJ_ARRAY */
            int count;
        } array;
        struct {
            nyxj_pair* pairs;       /* when type == NYXJ_OBJECT: a linked list of members */
            int count;
        } object;
    } as;
};

typedef struct nyxj_pair {
    char* key;
    nyxj_value* value;
    struct nyxj_pair* next;
} nyxj_pair;
```

- `type` says which member of the `as` union is meaningful. Always check it before reading `as`.
- Numbers are stored as `double`, like in JavaScript. Integers are exact up to 2^53 (about 9 * 10^15).
- Strings are UTF-8 and end with a `'\0'`, so they work with every standard C string function.
- Arrays are a C array of pointers with a `count`.
- Objects are a linked list of `nyxj_pair` (key, value, next), kept in document order.

You can read these fields directly. To **create or modify** values, always use the builder functions of
Chapter 9: they maintain internal fields that hand-made values would get wrong.

### Walking a tree

The natural way to process a tree is a recursive function. This program prints every value with its
type:

```c
#include "nyxj.h"
#include <stdio.h>

static void indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

static void print_value(const nyxj_value* v, int depth) {
    indent(depth);
    switch (v->type) {
        case NYXJ_NULL:   printf("null\n"); break;
        case NYXJ_BOOL:   printf("bool: %s\n", v->as.boolean ? "true" : "false"); break;
        case NYXJ_NUMBER: printf("number: %g\n", v->as.number); break;
        case NYXJ_STRING: printf("string: \"%s\"\n", v->as.string); break;
        case NYXJ_ARRAY:
            printf("array with %d item(s)\n", v->as.array.count);
            for (int i = 0; i < v->as.array.count; i++) print_value(v->as.array.items[i], depth + 1);
            break;
        case NYXJ_OBJECT:
            printf("object with %d member(s)\n", v->as.object.count);
            for (const nyxj_pair* p = v->as.object.pairs; p; p = p->next) {
                indent(depth + 1);
                printf("\"%s\":\n", p->key);
                print_value(p->value, depth + 2);
            }
            break;
    }
}

int main(void) {
    nyxj_result r = nyxj_parse("{\"name\": \"Ada\", \"admin\": true, \"tags\": [1, null]}");
    if (!r.is_valid) return 1;
    print_value(r.root, 0);
    nyxj_free_value(r.root);
    return 0;
}
```

```text
object with 3 member(s)
  "name":
    string: "Ada"
  "admin":
    bool: true
  "tags":
    array with 2 item(s)
      number: 1
      null
```

### Ownership: who frees what?

C has no garbage collector, so every piece of memory must be released exactly once. NyxJ follows three
simple rules, and every header comment tells you which rule applies:

**Rule 1: "caller frees".** Functions that create something give it to you. You must free it.

| Returned thing | Examples | Free it with |
|---|---|---|
| A new tree (`nyxj_value*`) | `nyxj_parse*` (`r.root`), `nyxj_new_*`, `nyxj_clone`, `nyxj_query`, `nyxj_diff`, `nyxj_kv_get`, `nyxj_msgpack_decode`, `*_detach` | `nyxj_free_value()` |
| A buffer (`char*`, `uint8_t*`) | `nyxj_serialize*`, `nyxj_repair`, `nyxj_msgpack_encode`, `nyxj_read_file`, `nyxj_metrics_render` | `free()` |

**Rule 2: "borrowed".** Functions that *look up* something return a pointer *inside* an existing tree.
Never free it. It stays valid as long as the tree exists and you do not remove that part of it.

Examples: `nyxj_get_member`, `nyxj_get_path`, `nyxj_array_get`, `nyxj_get_string`, `nyxj_path_string`,
`nyxj_pointer_get`, `nyxj_query_first`, `nyxj_req_json`.

**Rule 3: "takes ownership".** Functions that *insert* a value into a container take it over. After a
successful call, the container will free it: you must not free it yourself.

Examples: `nyxj_array_push`, `nyxj_array_insert`, `nyxj_object_set`, `nyxj_res_json`. If such a call
returns `false` (out of memory, wrong container type), you still own the value and must free it.

Freeing a container frees everything inside it. So in practice you free only the **roots** of your
trees.

### Classic memory mistakes

```c
nyxj_result r = nyxj_parse(json);
const char* name = nyxj_get_string(r.root, "name");  /* borrowed from r.root */
nyxj_free_value(r.root);
printf("%s\n", name);            /* WRONG: use after free, name pointed inside the freed tree */

nyxj_value* child = nyxj_get_member(r.root, "address");
nyxj_free_value(child);          /* WRONG: child is borrowed, the parent will free it again */

nyxj_value* s = nyxj_new_string("x");
nyxj_array_push(arr, s);
nyxj_free_value(s);              /* WRONG: arr owns s now */
```

To keep a borrowed value after the tree is freed, copy it first: `strdup()` for a string,
`nyxj_clone()` for a value (and free the copy later).

**Tip**: build your programs with `-fsanitize=address` (GCC and Clang on Linux and macOS) while
developing. AddressSanitizer reports every leak, double free and use after free with the exact line.

### Recap

- A `nyxj_value` has a `type` and a union `as`; objects are linked lists of pairs in document order.
- Read fields directly, but create and modify values only through the builder API.
- Caller frees what is created; borrowed pointers are never freed; inserted values belong to their
  container.
- Free only the roots. `nyxj_free_value(NULL)` is allowed and does nothing.

---

## Chapter 7. Parsing in Depth

### Four ways to parse

```c
nyxj_result r1 = nyxj_parse(text);                 /* NUL-terminated string */
nyxj_result r2 = nyxj_parse_n(buffer, length);     /* explicit length: the buffer needs no '\0' */
nyxj_result r3 = nyxj_parse_ex(buffer, length, &options);  /* with options and limits */

nyxj_error err;
nyxj_value* doc = nyxj_parse_file("config.json", &options, &err);  /* reads and parses a file */
if (!doc) fprintf(stderr, "%s\n", err.message);
```

- Use `nyxj_parse_n` for network buffers or memory-mapped files, which are rarely NUL-terminated.
- `nyxj_parse_file` returns the root directly (or `NULL` and fills `err`). Pass `NULL` as options to use
  the defaults.
- `nyxj_read_file(path, &len, &err)` only reads a file into a buffer (caller frees), if you want to parse
  it yourself.

### Parse options

```c
nyxj_parse_options opts = nyxj_parse_options_default();
```

| Field | Default | Meaning |
|---|---|---|
| `flags` | `0` | Syntax extensions and checks, see below |
| `max_depth` | `512` | Maximum nesting of arrays and objects |
| `max_input_size` | `0` (unlimited) | Maximum input size in bytes |
| `max_string_length` | `0` (unlimited) | Maximum bytes per string |
| `max_elements` | `0` (unlimited) | Maximum items per array or members per object |
| `arena` | `NULL` | Allocate the tree in an arena (Chapter 10) |

Always start from `nyxj_parse_options_default()` and change the fields you need: new fields added in
later versions will then keep sensible defaults.

### Flags

| Flag | Effect |
|---|---|
| `NYXJ_PARSE_ALLOW_COMMENTS` | Accept `// line` and `/* block */` comments |
| `NYXJ_PARSE_ALLOW_TRAILING_COMMAS` | Accept `[1, 2,]` and `{"a": 1,}` |
| `NYXJ_PARSE_RELAXED` | Both of the above: handy for hand-written configuration files |
| `NYXJ_PARSE_REJECT_DUPLICATE_KEYS` | Fail on `{"a": 1, "a": 2}` |

By default, duplicate keys are accepted and kept; getters then return the first one. Reject them when the
data comes from an untrusted source: two systems that pick different duplicates can be tricked into
disagreeing about the content of the same document.

### Limits: protection against JSON denial of service

An attacker does not need a bug in your code to hurt you: a valid but abusive document can be enough.

- `[[[[[[...]]]]]]` nested 1 000 000 times exhausts the stack of a naive recursive parser.
- A 2 GB string, or an array of 50 million zeros, exhausts memory.

NyxJ always enforces `max_depth` (512 by default). For data that comes from the network, also set the
other limits to values that make sense for your application:

```c
nyxj_parse_options opts = nyxj_parse_options_default();
opts.max_depth = 32;
opts.max_input_size = 1024 * 1024;   /* 1 MiB */
opts.max_string_length = 64 * 1024;  /* 64 KiB */
opts.max_elements = 10000;
```

A document that crosses a limit fails with `NYXJ_ERR_DEPTH_LIMIT`, `NYXJ_ERR_SIZE_LIMIT`,
`NYXJ_ERR_STRING_LIMIT` or `NYXJ_ERR_ELEMENT_LIMIT`, before any harm is done.

### Errors

When `is_valid` is `false`, `r.error` (a `nyxj_error`) tells you everything:

| Field | Meaning |
|---|---|
| `code` | An `nyxj_error_code` such as `NYXJ_ERR_UNEXPECTED_CHAR` |
| `offset` | Byte offset of the problem in the input (0-based) |
| `line`, `column` | Position of the problem (1-based; the column counts bytes) |
| `message` | A complete human-readable message, including the position |

Two helpers turn a code into text: `nyxj_error_name(code)` gives a short name (`"DEPTH_LIMIT"`) and
`nyxj_strerror(code)` a description (`"maximum nesting depth exceeded"`). The complete list of codes is
in Appendix A. (`r.error_msg` is a short static description kept for compatibility with NyxJ 1.x.)

### Example: a configuration file with friendly errors

This program parses a hand-written configuration, first strictly, then in relaxed mode, and shows how to
point at the exact location of an error:

```c
#include "nyxj.h"
#include <stdio.h>
#include <string.h>

/* Prints the error, the faulty line and a caret under the column. */
static void show_error(const char* text, const nyxj_error* e) {
    printf("error %s: %s\n", nyxj_error_name(e->code), e->message);
    const char* line = text + e->offset;
    while (line > text && line[-1] != '\n') line--;
    const char* end = strchr(line, '\n');
    int len = end ? (int)(end - line) : (int)strlen(line);
    printf("  %.*s\n  %*s^\n", len, line, e->column - 1, "");
}

int main(void) {
    const char* config =
        "{\n"
        "  // Server settings\n"
        "  \"port\": 8080,\n"
        "  \"workers\": 4,\n"
        "}\n";

    nyxj_result strict = nyxj_parse(config);
    if (!strict.is_valid) show_error(config, &strict.error);

    nyxj_parse_options opts = nyxj_parse_options_default();
    opts.flags = NYXJ_PARSE_RELAXED;   /* comments and trailing commas */
    opts.max_depth = 32;               /* a configuration is never deeply nested */
    nyxj_result relaxed = nyxj_parse_ex(config, strlen(config), &opts);
    if (!relaxed.is_valid) {
        show_error(config, &relaxed.error);
        return 1;
    }
    printf("port=%lld workers=%lld\n", (long long)nyxj_get_int(relaxed.root, "port"),
           (long long)nyxj_get_int(relaxed.root, "workers"));
    nyxj_free_value(relaxed.root);
    return 0;
}
```

```text
error UNEXPECTED_CHAR: unexpected character '/' at line 2, column 3 (expected a string key)
    // Server settings
    ^
port=8080 workers=4
```

### Thread safety and speed

- The parser has **no global state**. Any number of threads can parse different documents at the same
  time without locks. (Modifying one tree from several threads at once, however, needs your own lock.)
- Whitespace skipping and string scanning use SIMD instructions (SSE2 or NEON) automatically;
  `nyxj_simd_backend()` tells you which one is active. There is nothing to configure.
- Every string is checked for valid UTF-8, escape sequences (including `😀` surrogate pairs) are
  decoded, and numbers follow the exact JSON grammar.

### Recap

- `nyxj_parse` for strings, `nyxj_parse_n` for buffers, `nyxj_parse_ex` for options,
  `nyxj_parse_file` for files.
- `NYXJ_PARSE_RELAXED` accepts comments and trailing commas; duplicate keys can be rejected.
- Set limits for untrusted input: depth, size, string length and element count.
- `nyxj_error` gives the code, offset, line, column and a full message.

---

## Chapter 8. Reading Data

### Getters

The simplest way to read an object member is a typed getter:

```c
const char* name  = nyxj_get_string(obj, "name");   /* NULL if absent or not a string */
double      price = nyxj_get_number(obj, "price");  /* 0.0 if absent or not a number */
int64_t     id    = nyxj_get_int(obj, "id");        /* 0 if absent; decimals are truncated */
bool        admin = nyxj_get_bool(obj, "admin");    /* false if absent or not a boolean */
nyxj_value* addr  = nyxj_get_member(obj, "address"); /* any type, NULL if absent */
```

Getters never crash: if `obj` is `NULL` or not an object, they return the default too. That lets you
chain lookups safely.

**Pitfall**: `nyxj_get_number` returns `0.0` both for `"price": 0` and for a missing price. When the
difference matters, test presence first with `nyxj_get_member(obj, "price") != NULL`, or use a path
function with a fallback value you can recognize.

### Paths: deep access in one line

Instead of chaining getters, describe where the value is:

```c
nyxj_value* v = nyxj_get_path(root, "orders[0].items[2].sku");
const char* city = nyxj_path_string(root, "user.address.city", "unknown");
double total     = nyxj_path_number(root, "orders[0].total", 0.0);
bool vip         = nyxj_path_bool(root, "user.vip", false);
```

Path syntax:

| Syntax | Meaning |
|---|---|
| `name` | Member `name` of an object |
| `a.b.c` | Nested members |
| `[3]` | Item at index 3 of an array (0-based) |
| `[-1]` | Last item (negative indexes count from the end) |
| `$.a` or `$a` | An optional leading `$` is accepted |

The `nyxj_path_*` functions return the fallback when the path does not exist **or** the value has
another type.

Keys containing dots or brackets cannot be written in a dot path. For those, use `nyxj_get_member`, or a
JSON Pointer (`nyxj_pointer_get`, Chapter 15), or NyxQuery's bracket notation (Chapter 14).

### Arrays

```c
nyxj_value* items = nyxj_get_member(root, "items");
for (int i = 0; i < nyxj_size(items); i++) {
    nyxj_value* item = nyxj_array_get(items, i);   /* borrowed, NULL if out of range */
    ...
}
nyxj_value* last = nyxj_array_get(items, -1);
```

`nyxj_size(v)` returns the number of items of an array, of members of an object, or of bytes of a
string (and `0` for `NULL`).

### Checking types

```c
if (v && v->type == NYXJ_STRING) { ... }
printf("%s\n", nyxj_type_name(v->type));   /* "string" */
```

### Example: an order summary

```c
#include "nyxj.h"
#include <stdio.h>

int main(void) {
    const char* json =
        "{\"customer\": {\"name\": \"Ada\", \"vip\": true},"
        " \"items\": [{\"sku\": \"NX-1\", \"price\": 12.5, \"qty\": 2},"
        "             {\"sku\": \"NX-7\", \"price\": 3.0, \"qty\": 5}]}";
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) return 1;

    printf("customer: %s\n", nyxj_path_string(r.root, "customer.name", "?"));
    printf("vip: %s\n", nyxj_path_bool(r.root, "customer.vip", false) ? "yes" : "no");
    printf("last sku: %s\n", nyxj_path_string(r.root, "items[-1].sku", "?"));
    printf("coupon: %s\n", nyxj_path_string(r.root, "coupon.code", "(none)"));

    double total = 0;
    nyxj_value* items = nyxj_get_member(r.root, "items");
    for (int i = 0; i < nyxj_size(items); i++) {
        nyxj_value* it = nyxj_array_get(items, i);
        total += nyxj_get_number(it, "price") * (double)nyxj_get_int(it, "qty");
    }
    printf("total: %.2f\n", total);

    nyxj_free_value(r.root);
    return 0;
}
```

```text
customer: Ada
vip: yes
last sku: NX-7
coupon: (none)
total: 40.00
```

### Recap

- Typed getters return a safe default (`NULL`, `0`, `false`) when a member is missing.
- Paths such as `"orders[0].items[-1].sku"` reach deep values in one call; `nyxj_path_*` adds a fallback.
- `nyxj_array_get` accepts negative indexes; `nyxj_size` counts items, members or bytes.

---

## Chapter 9. Building and Writing JSON

### Creating values

```c
nyxj_value* n = nyxj_new_null();
nyxj_value* b = nyxj_new_bool(true);
nyxj_value* x = nyxj_new_number(3.14);
nyxj_value* s = nyxj_new_string("hello");          /* copies the string */
nyxj_value* t = nyxj_new_stringn(buffer, 5);       /* copies exactly 5 bytes */
nyxj_value* a = nyxj_new_array();
nyxj_value* o = nyxj_new_object();
```

Each constructor returns a new value that you own (or `NULL` if memory runs out).

### Filling objects

```c
nyxj_object_set(o, "tags", a);                 /* takes ownership of a */
nyxj_object_set_string(o, "name", "NyxJ");     /* creates the child for you */
nyxj_object_set_number(o, "version", 2);
nyxj_object_set_bool(o, "fast", true);
nyxj_object_set_null(o, "deprecated");

nyxj_object_remove(o, "deprecated");           /* removes and frees the member */
nyxj_value* tags = nyxj_object_detach(o, "tags");  /* removes it and gives it back to you */
```

Setting a key that already exists **replaces** its value (the old value is freed) and keeps the member
at its position.

### Filling arrays

```c
nyxj_array_push(a, nyxj_new_string("fast"));       /* append, takes ownership */
nyxj_array_insert(a, 0, nyxj_new_string("first")); /* insert at index 0 */
nyxj_array_remove(a, 1);                           /* remove and free item 1 */
nyxj_value* item = nyxj_array_detach(a, 0);        /* remove item 0 and give it back */
```

### Copying and comparing

```c
nyxj_value* copy = nyxj_clone(o);   /* deep copy, you own it */
bool same = nyxj_equals(o, copy);   /* deep comparison */
```

`nyxj_equals` compares arrays item by item, and objects member by member regardless of member order:
`{"a":1,"b":2}` equals `{"b":2,"a":1}`.

### Serializing

```c
char* compact = nyxj_serialize(v);             /* {"a":1,"b":[true,null]} */
char* pretty  = nyxj_serialize_pretty(v, 2);   /* indented with 2 spaces */

nyxj_serialize_options so = {0};
so.indent = 0;            /* 0 = compact, N = pretty with N spaces */
so.sort_keys = true;      /* keys in byte order: canonical output, stable for hashing and diffs */
so.escape_unicode = true; /* write non-ASCII characters as \uXXXX (pure ASCII output) */
size_t len;
char* canonical = nyxj_serialize_ex(v, &so, &len);  /* len receives the length */

free(compact); free(pretty); free(canonical);
```

What the serializer guarantees:

- Output is always valid JSON: quotes, backslashes and control characters in strings are escaped.
- Numbers use the shortest text that reads back to the exact same `double` (`0.1 + 0.2` is written
  `0.30000000000000004`, `42.0` is written `42`). `NaN` and infinities, which JSON cannot represent, are
  written `null`.
- Serialization runs in linear time, whatever the size of the tree.

### Example: build, modify, print

```c
#include "nyxj.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    nyxj_value* user = nyxj_new_object();
    nyxj_object_set_string(user, "name", "Ada");
    nyxj_object_set_number(user, "age", 36);
    nyxj_object_set_bool(user, "admin", false);
    nyxj_object_set_null(user, "email");

    nyxj_value* langs = nyxj_new_array();
    nyxj_array_push(langs, nyxj_new_string("c"));
    nyxj_array_push(langs, nyxj_new_string("go"));
    nyxj_array_insert(langs, 0, nyxj_new_string("asm"));
    nyxj_object_set(user, "langs", langs);     /* user now owns langs */

    nyxj_object_set_number(user, "age", 37);   /* replaces the old value */
    nyxj_object_remove(user, "email");

    char* compact = nyxj_serialize(user);
    char* pretty = nyxj_serialize_pretty(user, 2);
    nyxj_serialize_options canon = {0};
    canon.sort_keys = true;
    char* sorted = nyxj_serialize_ex(user, &canon, NULL);
    printf("%s\n%s\n%s\n", compact, pretty, sorted);
    free(compact);
    free(pretty);
    free(sorted);

    nyxj_value* copy = nyxj_clone(user);
    printf("equal: %s\n", nyxj_equals(user, copy) ? "yes" : "no");
    nyxj_object_set_bool(copy, "admin", true);
    printf("equal after change: %s\n", nyxj_equals(user, copy) ? "yes" : "no");

    nyxj_free_value(copy);
    nyxj_free_value(user);                     /* frees langs too */
    return 0;
}
```

```text
{"name":"Ada","age":37,"admin":false,"langs":["asm","c","go"]}
{
  "name": "Ada",
  "age": 37,
  "admin": false,
  "langs": [
    "asm",
    "c",
    "go"
  ]
}
{"admin":false,"age":37,"langs":["asm","c","go"],"name":"Ada"}
equal: yes
equal after change: no
```

### Writing to a file

```c
char* text = nyxj_serialize_pretty(doc, 2);
FILE* f = fopen("out.json", "wb");
if (f && text) fputs(text, f);
if (f) fclose(f);
free(text);
```

### Recap

- `nyxj_new_*` create values; `nyxj_object_set*` and `nyxj_array_push/insert` fill containers.
- Setting an existing key replaces the value in place; `remove` frees, `detach` gives the value back.
- `nyxj_serialize` (compact), `nyxj_serialize_pretty` (indented), `nyxj_serialize_ex` (sorted keys,
  ASCII output, length). Free the returned text with `free()`.

---

## Chapter 10. The Arena Allocator

### The problem

A parsed tree is made of many small pieces: one allocation per value, per string, per key. Allocating and
freeing millions of small blocks costs time and fragments memory, which matters for a server handling
thousands of messages per second or for a device with little RAM.

### The solution

An **arena** (or memory pool) grabs memory in big blocks and hands out pieces of them by simply moving a
pointer forward. Nothing is freed individually: when you are done with everything, you **reset** the
arena and all of it is released at once, in constant time.

```c
nyxj_arena* arena = nyxj_arena_create(64 * 1024);   /* block size: 64 KiB */
nyxj_parse_options opts = nyxj_parse_options_default();
opts.arena = arena;                                  /* the tree goes into the arena */
```

### Example: a message loop

```c
#include "nyxj.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* messages[] = {
        "{\"sensor\": \"t1\", \"celsius\": 21.5}",
        "{\"sensor\": \"t2\", \"celsius\": 19.0}",
        "{\"sensor\": \"t3\", \"celsius\": 23.25}",
    };
    nyxj_arena* arena = nyxj_arena_create(64 * 1024);
    nyxj_parse_options opts = nyxj_parse_options_default();
    opts.arena = arena;

    for (int i = 0; i < 3; i++) {
        nyxj_result r = nyxj_parse_ex(messages[i], strlen(messages[i]), &opts);
        if (r.is_valid)
            printf("%s: %.2f C (arena: %zu bytes used)\n", nyxj_get_string(r.root, "sensor"),
                   nyxj_get_number(r.root, "celsius"), nyxj_arena_used(arena));
        nyxj_arena_reset(arena);   /* releases the whole tree at once */
    }
    nyxj_arena_destroy(arena);
    return 0;
}
```

```text
t1: 21.50 C (arena: 208 bytes used)
t2: 19.00 C (arena: 208 bytes used)
t3: 23.25 C (arena: 208 bytes used)
```

The usage never grows: after each reset, the next tree is built in the same memory.

### Rules for arena trees

1. **Read-only**: do not modify an arena tree with the builder functions. Clone it with `nyxj_clone()`
   to get a normal, modifiable heap copy.
2. **No individual free**: never call `nyxj_free_value` on an arena tree; reset or destroy the arena.
3. **Lifetime**: every pointer into the tree becomes invalid after `nyxj_arena_reset` or
   `nyxj_arena_destroy`.
4. **One arena per thread**: an arena is not thread-safe. Give each worker thread its own.

You can also store your own data in an arena with `nyxj_arena_alloc(arena, size)` and
`nyxj_arena_strndup(arena, s, len)`; it is released with the rest.

### Recap

- An arena allocates in big blocks and releases everything at once, in O(1).
- Set `opts.arena` to parse into it; reset it between messages.
- Arena trees are read-only, never freed individually, and invalid after a reset.

---

## Chapter 11. Logging

NyxJ includes a small structured logger, used by the engine itself and available to your programs
through `nyxj_log.h`.

### Levels

From most to least verbose: `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL` (and `OFF` to silence
everything). Messages below the current level are ignored. The default level is `INFO`.

```c
#include "nyxj_log.h"

nyxj_log_set_level(NYXJ_LOG_DEBUG);
NYXJ_INFO("orders", "processed %d orders in %.1f ms", count, ms);
NYXJ_WARN("payments", "retrying gateway call (attempt %d)", attempt);
```

The first argument is a **module name** that tells you which part of the program wrote the line. The
rest works like `printf`.

### Output options

```c
nyxj_log_set_stream(stdout);              /* default: stderr */
nyxj_log_set_json(true);                  /* one JSON object per line, for log collectors */
nyxj_log_set_sink(my_function, my_data);  /* send lines anywhere (file, syslog, network...) */
nyxj_log_set_sink(NULL, NULL);            /* back to the stream */

nyxj_log_level level;
if (nyxj_log_level_parse("warn", &level)) nyxj_log_set_level(level);   /* from a config value */
```

### Example

```c
#include "nyxj_log.h"
#include <stdio.h>

static void my_sink(nyxj_log_level level, const char* module, const char* message, void* user) {
    (void)user;
    printf("[sink] %s %s: %s\n", nyxj_log_level_name(level), module, message);
}

int main(void) {
    nyxj_log_set_level(NYXJ_LOG_DEBUG);
    nyxj_log_set_stream(stdout);
    NYXJ_INFO("orders", "processed %d orders in %.1f ms", 42, 3.5);
    NYXJ_DEBUG("orders", "cache hit ratio %.2f", 0.97);
    NYXJ_TRACE("orders", "this line is filtered out");

    nyxj_log_set_json(true);
    NYXJ_WARN("payments", "retrying gateway call (attempt %d)", 2);

    nyxj_log_set_sink(my_sink, NULL);
    NYXJ_ERROR("db", "connection lost");
    return 0;
}
```

```text
2026-11-02T14:42:29.888Z INFO  [orders] processed 42 orders in 3.5 ms
2026-11-02T14:42:29.888Z DEBUG [orders] cache hit ratio 0.97
{"ts":"2026-11-02T14:42:29.888Z","level":"WARN","module":"payments","msg":"retrying gateway call (attempt 2)","src":"main.c:17"}
[sink] ERROR db: connection lost
```

Text lines start with a UTC timestamp, the level and the module; JSON lines also give the source file
and line. The `TRACE` line is filtered out because the level is `DEBUG`.

### Removing logs at compile time

Each `NYXJ_DEBUG(...)` call costs a level check even when it is filtered. For release builds, remove the
verbose calls entirely:

```bash
gcc -DNYXJ_LOG_COMPILE_LEVEL=2 ...   # strips TRACE (0) and DEBUG (1) from the binary
```

### Recap

- `NYXJ_INFO("module", "format", ...)` and friends; filter with `nyxj_log_set_level`.
- Text or JSON lines, to any stream or to your own sink.
- `-DNYXJ_LOG_COMPILE_LEVEL=N` removes lower levels from the binary.

---

# Part III. Data Modules

## Chapter 12. Mapping JSON to C Structs

### The problem

Real programs work with their own types (`User`, `Order`, `Config`), not with generic trees. Copying each
field by hand (`u.id = nyxj_get_int(...)`, `u.name = strdup(...)`...) is long, repetitive and easy to get
wrong, especially the error handling.

### The solution: describe once, map automatically

You describe each field of your struct in a **field table**; NyxJ then fills the struct from JSON, checks
the types, reports missing fields and can even go the other way (struct to JSON).

```c
typedef struct {
    int id;
    char* name;
    double score;
    bool active;
} User;

static const nyxj_field user_fields[] = {
    /*               struct, member, JSON key, required? */
    NYXJ_FIELD_INT(User, id, "id", true),
    NYXJ_FIELD_STRING(User, name, "name", true),
    NYXJ_FIELD_DOUBLE(User, score, "score", false),
    NYXJ_FIELD_BOOL(User, active, "active", false),
};
```

Each macro records the member's position and size inside the struct (with `offsetof`), the JSON key, the
expected kind and whether the field is required.

### Field kinds

| Macro | C member type | Notes |
|---|---|---|
| `NYXJ_FIELD_INT` | `int` | Fails if the number is not an integer or out of range |
| `NYXJ_FIELD_INT64` | `int64_t` | Large integers (up to 2^53 exactly) |
| `NYXJ_FIELD_DOUBLE` | `double` | |
| `NYXJ_FIELD_FLOAT` | `float` | |
| `NYXJ_FIELD_BOOL` | `bool` | |
| `NYXJ_FIELD_STRING` | `char*` | Heap copy, released by `nyxj_map_free` |
| `NYXJ_FIELD_STRBUF` | `char[N]` | Fixed buffer, no allocation; a string too long is an error |
| `NYXJ_FIELD_OBJECT` | a nested struct | Takes one more argument: the nested field table |
| `NYXJ_FIELD_VALUE` | `nyxj_value*` | Any JSON (arrays, free-form data), deep copy released by `nyxj_map_free` |

### The functions

```c
bool  nyxj_map_parse(json, fields, count, &out, &err);        /* parse + map */
bool  nyxj_map_to_struct(value, fields, count, &out, &err);   /* map an existing tree */
char* nyxj_struct_to_json(&in, fields, count, indent);        /* struct -> JSON text (caller frees) */
nyxj_value* nyxj_struct_to_value(&in, fields, count);         /* struct -> tree */
void  nyxj_map_free(&s, fields, count);                       /* frees STRING and VALUE members */
```

`count` is the number of entries in the table: use `NYXJ_FIELDS_COUNT(table)`.

Important behaviors:

- **Initialize your struct first** (`User u = {0};`). Optional fields that are missing from the JSON keep
  the value you put there, which is a simple way to define defaults.
- Unknown JSON keys are ignored.
- On failure, `err.message` names the faulty field with its full path, such as
  `address.city: expected a string, got number`. Fields mapped before the error may already hold heap
  copies, so call `nyxj_map_free` in both cases.

### Example: a customer record

```c
#include "nyxj.h"
#include "nyxj_map.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char city[32];
    char country[3];
} Address;

typedef struct {
    int id;
    char* name;
    double balance;
    bool active;
    Address address;
    nyxj_value* tags;   /* any JSON, kept as a value */
} Customer;

static const nyxj_field address_fields[] = {
    NYXJ_FIELD_STRBUF(Address, city, "city", true),
    NYXJ_FIELD_STRBUF(Address, country, "country", true),
};

static const nyxj_field customer_fields[] = {
    NYXJ_FIELD_INT(Customer, id, "id", true),
    NYXJ_FIELD_STRING(Customer, name, "name", true),
    NYXJ_FIELD_DOUBLE(Customer, balance, "balance", false),
    NYXJ_FIELD_BOOL(Customer, active, "active", false),
    NYXJ_FIELD_OBJECT(Customer, address, "address", true, address_fields),
    NYXJ_FIELD_VALUE(Customer, tags, "tags", false),
};
#define CUSTOMER_FIELDS customer_fields, NYXJ_FIELDS_COUNT(customer_fields)

int main(void) {
    const char* json = "{\"id\": 7, \"name\": \"Ada\", \"active\": true,"
                       " \"address\": {\"city\": \"Kinshasa\", \"country\": \"CD\"}, \"tags\": [\"vip\"]}";
    Customer c = {0};
    c.balance = 100.0;   /* default, kept because "balance" is absent */
    nyxj_error err;
    if (!nyxj_map_parse(json, CUSTOMER_FIELDS, &c, &err)) {
        printf("mapping failed: %s\n", err.message);
        nyxj_map_free(&c, CUSTOMER_FIELDS);
        return 1;
    }
    printf("#%d %s, %s/%s, balance %.2f, %d tag(s)\n", c.id, c.name, c.address.city, c.address.country,
           c.balance, nyxj_size(c.tags));

    c.balance -= 25;
    char* back = nyxj_struct_to_json(&c, CUSTOMER_FIELDS, 0);
    printf("%s\n", back);
    free(back);
    nyxj_map_free(&c, CUSTOMER_FIELDS);

    Customer bad = {0};
    if (!nyxj_map_parse("{\"id\": \"seven\", \"name\": \"Bob\"}", CUSTOMER_FIELDS, &bad, &err))
        printf("mapping failed: %s [%s]\n", err.message, nyxj_error_name(err.code));
    nyxj_map_free(&bad, CUSTOMER_FIELDS);
    return 0;
}
```

```text
#7 Ada, Kinshasa/CD, balance 100.00, 1 tag(s)
{"id":7,"name":"Ada","balance":75,"active":true,"address":{"city":"Kinshasa","country":"CD"},"tags":["vip"]}
mapping failed: id: expected an integer, got string [TYPE_MISMATCH]
```

**Tip**: the `CUSTOMER_FIELDS` macro expands to "table, count", which saves typing both arguments every
time.

### Let NyxJ write the tables: gen-struct

Writing tables by hand for a large document is tedious. Give a sample document to `nyxj gen-struct` (or
call `nyxj_generate_struct(sample, "TypeName")` from C) and it writes the structs and tables for you:

```bash
nyxj gen-struct --name Customer sample.json > customer.h
```

The generator uses `char*` for strings, `nyxj_value*` for arrays, a nested struct for each nested object,
and marks every field present in the sample as required. Review the result and adjust it: switch a
string to `STRBUF`, make a field optional, rename a type.

### Recap

- Describe struct members once in a `nyxj_field` table, then call `nyxj_map_parse`.
- Nine kinds, including nested structs (`OBJECT`) and free-form JSON (`VALUE`).
- Initialize the struct to set defaults; always call `nyxj_map_free`.
- `nyxj_struct_to_json` goes back to JSON; `nyxj gen-struct` writes tables from a sample.

---

## Chapter 13. Validating with JSON Schema

### What a schema is

A **JSON Schema** is a JSON document that describes what another JSON document must look like: which
members are required, their types, the allowed ranges and formats. Validating input against a schema at
the edge of your program means the rest of the code can trust the data.

```json
{
  "type": "object",
  "required": ["email", "age"],
  "properties": {
    "email": { "type": "string", "format": "email" },
    "age":   { "type": "integer", "minimum": 13, "maximum": 130 },
    "plan":  { "enum": ["free", "pro"] }
  },
  "additionalProperties": false
}
```

This reads: "an object with mandatory `email` (a string that looks like an email address) and `age` (an
integer from 13 to 130), an optional `plan` that is either `free` or `pro`, and no other member".

### Supported keywords

NyxJ implements the practical core of JSON Schema (drafts 4 to 2020-12):

| Purpose | Keywords |
|---|---|
| Types and values | `type`, `enum`, `const` |
| Combining schemas | `allOf`, `anyOf`, `oneOf`, `not`, `if` / `then` / `else`, `$ref` (local, for example `#/$defs/address`) |
| Objects | `properties`, `required`, `additionalProperties`, `minProperties`, `maxProperties` |
| Arrays | `items`, `prefixItems`, `contains`, `minItems`, `maxItems`, `uniqueItems` |
| Numbers | `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`, `multipleOf` |
| Strings | `minLength`, `maxLength`, `format` (`email`, `uuid`, `ipv4`, `date`, `date-time`) |

Unknown keywords (such as `title` or `description`) are ignored, so documented schemas work as they are.

### The functions

```c
nyxj_schema_report report;
bool ok = nyxj_schema_validate(schema, document, &report);     /* two parsed trees */
bool ok2 = nyxj_schema_validate_json(schema_text, doc_text, &report, &err);  /* parses both for you */
nyxj_value* list = nyxj_schema_report_to_value(&report);       /* issues as a JSON array */
```

The report lists every problem found, not just the first one:

| Field | Meaning |
|---|---|
| `report.count` | Number of issues stored in `report.issues` (at most `NYXJ_SCHEMA_MAX_ISSUES`, 32) |
| `report.total` | Number of issues found (can be larger than `count`) |
| `issues[i].path` | Where in the document, for example `$.user.age` |
| `issues[i].keyword` | Which rule failed, for example `minimum` |
| `issues[i].message` | A readable explanation |

Pass `NULL` as the report when you only need a yes or no answer.

### Example: validating sign-ups

```c
#include "nyxj.h"
#include "nyxj_schema.h"
#include <stdio.h>

static const char* SCHEMA =
    "{"
    "  \"type\": \"object\","
    "  \"required\": [\"email\", \"age\"],"
    "  \"properties\": {"
    "    \"email\": {\"type\": \"string\", \"format\": \"email\"},"
    "    \"age\": {\"type\": \"integer\", \"minimum\": 13, \"maximum\": 130},"
    "    \"plan\": {\"enum\": [\"free\", \"pro\"]},"
    "    \"tags\": {\"type\": \"array\", \"items\": {\"type\": \"string\"}, \"uniqueItems\": true}"
    "  },"
    "  \"additionalProperties\": false"
    "}";

static void check(const nyxj_value* schema, const char* json) {
    nyxj_result doc = nyxj_parse(json);
    if (!doc.is_valid) {
        printf("not JSON: %s\n", doc.error.message);
        return;
    }
    nyxj_schema_report rep;
    if (nyxj_schema_validate(schema, doc.root, &rep)) {
        printf("valid:   %s\n", json);
    } else {
        printf("invalid: %s\n", json);
        for (int i = 0; i < rep.count; i++)
            printf("  %s [%s] %s\n", rep.issues[i].path, rep.issues[i].keyword, rep.issues[i].message);
    }
    nyxj_free_value(doc.root);
}

int main(void) {
    nyxj_result schema = nyxj_parse(SCHEMA);
    if (!schema.is_valid) return 1;
    check(schema.root, "{\"email\": \"ada@example.com\", \"age\": 36, \"plan\": \"pro\"}");
    check(schema.root, "{\"email\": \"nope\", \"age\": 7, \"plan\": \"gold\", \"tags\": [\"a\", \"a\"], \"x\": 1}");
    nyxj_free_value(schema.root);
    return 0;
}
```

```text
valid:   {"email": "ada@example.com", "age": 36, "plan": "pro"}
invalid: {"email": "nope", "age": 7, "plan": "gold", "tags": ["a", "a"], "x": 1}
  $.email [format] string is not a valid 'email'
  $.age [minimum] 7 is less than the minimum 13
  $.plan [enum] value is not one of the allowed values
  $.tags [uniqueItems] items 0 and 1 are equal
  $.x [additionalProperties] property 'x' is not allowed
```

Every problem is reported in one pass, which lets you show the user all their mistakes at once.

**Tip**: parse the schema once at startup and reuse the tree for every document; validation does not
modify it, so several threads can share it.

### Recap

- A schema describes valid documents: types, required members, ranges, formats.
- `nyxj_schema_validate` returns `true`/`false` and fills a report with path, keyword and message.
- Validate at the edge of your program; the rest of the code can then trust the data.

---

## Chapter 14. Querying with NyxQuery

### Paths versus queries

A dot path (Chapter 8) leads to **one** value. A **query** searches the document and returns **every**
value that matches a pattern: all prices, all books cheaper than 10, all members named `id` at any depth.
NyxJ's query language, **NyxQuery**, follows the widely used **JSONPath** syntax.

### Syntax, by example

All examples run on this document:

```json
{"store": {
  "book": [
    {"title": "Dune", "author": "Herbert", "price": 8.99, "category": "fiction"},
    {"title": "SICP", "author": "Abelson", "price": 45, "category": "cs", "isbn": "0262510871"},
    {"title": "Neuromancer", "author": "Gibson", "price": 7.5, "category": "fiction"}
  ],
  "bicycle": {"color": "red", "price": 199}
}}
```

| Query | Meaning | Result |
|---|---|---|
| `$` | The root | an array holding the whole document |
| `$.store.book[0].title` | Child members and an index | `["Dune"]` |
| `$.store.book[-1].author` | Negative index: from the end | `["Gibson"]` |
| `$.store.book[0:2].title` | Slice `[start:end]` (end excluded) | `["Dune","SICP"]` |
| `$.store.book[::-1].title` | Slice with a step: reversed | `["Neuromancer","SICP","Dune"]` |
| `$.store.book[0,2].title` | Union of indexes | `["Dune","Neuromancer"]` |
| `$.store.bicycle.*` | Wildcard: every member | `["red",199]` |
| `$..price` | Recursive descent: at any depth | `[8.99,45,7.5,199]` |
| `$['store']['bicycle']` | Bracket notation (keys with special characters) | the bicycle object |
| `$..book[?(@.price < 10)].title` | Filter: `@` is the current item | `["Dune","Neuromancer"]` |
| `$..book[?(@.isbn)].title` | Existence test | `["SICP"]` |
| `$..book[?(@.category == 'fiction' && @.price > 8)].title` | Combined conditions | `["Dune"]` |

Filters support `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`, parentheses, numbers, strings in
single or double quotes, `true`, `false`, `null`, and paths starting with `@` (the current item) or `$`
(the root).

### The functions

```c
nyxj_value* all = nyxj_query(root, "$..price", &err);            /* array of copies: you free it */
const nyxj_value* first = nyxj_query_first(root, "$..isbn", &err); /* borrowed, NULL if none */
int n = nyxj_query_count(root, "$.store.book[*]", &err);           /* number of matches, -1 on error */
```

When you run the same query many times (for example on every request), compile it once:

```c
nyxj_compiled_query* q = nyxj_query_compile("$..price", &err);  /* NULL if the syntax is invalid */
int matches = nyxj_query_exec(q, root, callback, user_data);    /* calls callback for each match */
nyxj_query_free(q);
```

The callback receives each match (borrowed) and returns `true` to continue or `false` to stop early.
A compiled query is read-only, so several threads can share it.

### Example

```c
#include "nyxj.h"
#include "nyxj_query.h"
#include <stdio.h>
#include <stdlib.h>

static const char* STORE =
    "{\"store\": {\"book\": ["
    "  {\"title\": \"Dune\", \"author\": \"Herbert\", \"price\": 8.99, \"category\": \"fiction\"},"
    "  {\"title\": \"SICP\", \"author\": \"Abelson\", \"price\": 45, \"category\": \"cs\", \"isbn\": \"0262510871\"},"
    "  {\"title\": \"Neuromancer\", \"author\": \"Gibson\", \"price\": 7.5, \"category\": \"fiction\"}"
    "], \"bicycle\": {\"color\": \"red\", \"price\": 199}}}";

static void run(const nyxj_value* root, const char* path) {
    nyxj_error err;
    nyxj_value* matches = nyxj_query(root, path, &err);
    if (!matches) {
        printf("%s\n  error: %s\n", path, err.message);
        return;
    }
    char* text = nyxj_serialize(matches);
    printf("%s\n  -> %s\n", path, text);
    free(text);
    nyxj_free_value(matches);
}

static bool add_price(const nyxj_value* match, void* user) {
    *(double*)user += match->as.number;
    return true;   /* false would stop the iteration */
}

int main(void) {
    nyxj_result r = nyxj_parse(STORE);
    if (!r.is_valid) return 1;
    run(r.root, "$.store.book[0:2].title");
    run(r.root, "$..book[?(@.price < 10)].title");
    run(r.root, "$..book[?(@.category == 'fiction' && @.price > 8)].title");
    run(r.root, "$.store.book[?(@.price >");

    nyxj_error err;
    nyxj_compiled_query* q = nyxj_query_compile("$..price", &err);
    double total = 0;
    int n = nyxj_query_exec(q, r.root, add_price, &total);
    printf("%d prices, total %.2f\n", n, total);
    nyxj_query_free(q);

    printf("books: %d\n", nyxj_query_count(r.root, "$.store.book[*]", &err));
    nyxj_free_value(r.root);
    return 0;
}
```

```text
$.store.book[0:2].title
  -> ["Dune","SICP"]
$..book[?(@.price < 10)].title
  -> ["Dune","Neuromancer"]
$..book[?(@.category == 'fiction' && @.price > 8)].title
  -> ["Dune"]
$.store.book[?(@.price >
  error: expected an operand at position 24
4 prices, total 260.49
books: 3
```

### Recap

- NyxQuery is JSONPath: `$` root, `.key`, `[index]`, slices, `*`, `..`, unions and `?(...)` filters.
- `nyxj_query` returns copies to free; `nyxj_query_first` returns a borrowed pointer.
- Compile hot queries once with `nyxj_query_compile` and run them with `nyxj_query_exec`.

---

## Chapter 15. Tracking Changes: Pointer, Patch and Merge Patch

When a document changes, sending the whole new version is wasteful. It is better to send only the
**difference**. NyxJ implements the three standards that make this possible.

### JSON Pointer (RFC 6901): naming a location

A JSON Pointer is a string that designates one value: `/store/book/0/title`. Each `/` goes one level
down; array items are designated by their index. The empty string `""` is the whole document. In keys,
`~` is written `~0` and `/` is written `~1` (so the key `a/b` becomes `/a~1b`).

```c
nyxj_value* title = nyxj_pointer_get(root, "/store/book/0/title");   /* borrowed, NULL if absent */
```

### JSON Patch (RFC 6902): a list of operations

A JSON Patch is an array of operations, applied in order:

```json
[
  { "op": "replace", "path": "/age",     "value": 37 },
  { "op": "add",     "path": "/langs/1", "value": "go" },
  { "op": "remove",  "path": "/city" },
  { "op": "move",    "from": "/a", "path": "/b" },
  { "op": "copy",    "from": "/b", "path": "/c" },
  { "op": "test",    "path": "/name",    "value": "Ada" }
]
```

`test` does not change anything: it checks a value and makes the whole patch fail if it differs. It is
perfect to make sure you are patching the version you think you are patching.

### JSON Merge Patch (RFC 7386): a partial document

A merge patch is simpler: it looks like the document itself, containing only the members that change.
`null` means "delete this member".

```json
{ "age": 37, "langs": ["c", "go"], "city": null }
```

Its limits: it cannot set a member to `null` (null means delete), and it replaces arrays as a whole.

| | JSON Patch | Merge Patch |
|---|---|---|
| Readability | Operations | Looks like the document |
| Arrays | Item-level changes | Whole-array replacement |
| Conditions | `test` operation | None |
| Best for | Precise synchronization, audit logs | Simple partial updates (`PATCH` requests) |

### The functions

```c
nyxj_value* ops = nyxj_diff(old_doc, new_doc);          /* JSON Patch turning old into new (you free it) */
bool ok = nyxj_patch_apply(&doc, ops, &err);            /* atomic: on failure doc is unchanged */

nyxj_value* delta = nyxj_merge_diff(old_doc, new_doc);  /* Merge Patch (you free it) */
doc = nyxj_merge_patch(doc, delta);                     /* consumes doc, returns the merged document */
```

**Why atomic?** If the third of five operations fails, a half-applied patch would leave the document in
a state nobody intended. `nyxj_patch_apply` works on a copy and only swaps it in when every operation has
succeeded. That is why it takes a `nyxj_value**`: it may replace your root.

### Example

```c
#include "nyxj.h"
#include "nyxj_patch.h"
#include <stdio.h>
#include <stdlib.h>

static void print(const char* label, const nyxj_value* v) {
    char* s = nyxj_serialize(v);
    printf("%-12s %s\n", label, s);
    free(s);
}

int main(void) {
    nyxj_result v1 = nyxj_parse("{\"name\": \"Ada\", \"age\": 36, \"langs\": [\"c\"], \"city\": \"Paris\"}");
    nyxj_result v2 = nyxj_parse("{\"name\": \"Ada\", \"age\": 37, \"langs\": [\"c\", \"go\"]}");
    if (!v1.is_valid || !v2.is_valid) return 1;

    print("pointer", nyxj_pointer_get(v1.root, "/langs/0"));

    nyxj_value* patch = nyxj_diff(v1.root, v2.root);
    print("patch", patch);
    nyxj_value* doc = nyxj_clone(v1.root);
    nyxj_error err;
    if (nyxj_patch_apply(&doc, patch, &err)) print("patched", doc);
    printf("%-12s %s\n", "same as v2?", nyxj_equals(doc, v2.root) ? "yes" : "no");

    /* A failing "test" operation: the document is left untouched. */
    nyxj_result guarded = nyxj_parse("[{\"op\": \"test\", \"path\": \"/age\", \"value\": 99},"
                                     " {\"op\": \"remove\", \"path\": \"/name\"}]");
    if (!nyxj_patch_apply(&doc, guarded.root, &err)) printf("%-12s %s\n", "refused", err.message);
    print("unchanged", doc);

    nyxj_value* merge = nyxj_merge_diff(v1.root, v2.root);
    print("merge patch", merge);
    nyxj_value* merged = nyxj_merge_patch(nyxj_clone(v1.root), merge);  /* consumes the clone */
    print("merged", merged);

    nyxj_free_value(merged);
    nyxj_free_value(merge);
    nyxj_free_value(guarded.root);
    nyxj_free_value(doc);
    nyxj_free_value(patch);
    nyxj_free_value(v2.root);
    nyxj_free_value(v1.root);
    return 0;
}
```

```text
pointer      "c"
patch        [{"op":"remove","path":"/city"},{"op":"replace","path":"/age","value":37},{"op":"add","path":"/langs/1","value":"go"}]
patched      {"name":"Ada","langs":["c","go"],"age":37}
same as v2?  yes
refused      test: value at '/age' does not match
unchanged    {"name":"Ada","langs":["c","go"],"age":37}
merge patch  {"city":null,"age":37,"langs":["c","go"]}
merged       {"name":"Ada","age":37,"langs":["c","go"]}
```

The patched document lists `age` last because `replace` rewrites the member, but it is equal to `v2`:
member order does not matter in JSON.

### A typical use: state synchronization

A server keeps the last state it sent to each client. When the state changes, it computes
`nyxj_diff(last_sent, current)` and sends only that patch. The client applies it with
`nyxj_patch_apply`. A screen with 5 000 values where 3 change transfers 3 operations instead of 5 000
values.

### Recap

- JSON Pointer names one location: `/a/b/0`.
- JSON Patch is a list of operations (`add`, `remove`, `replace`, `move`, `copy`, `test`), applied
  atomically by `nyxj_patch_apply`.
- Merge Patch is a partial document where `null` deletes; `nyxj_merge_patch` consumes its target.
- `nyxj_diff` and `nyxj_merge_diff` compute the patches for you.

---

## Chapter 16. MessagePack

### What it is

**MessagePack** is a binary format with exactly the same data model as JSON (null, booleans, numbers,
strings, arrays, maps) but encoded in bytes instead of text. It is typically 20 to 50 percent smaller
and much faster to decode, because numbers are stored in binary and lengths are known in advance.

The price: it is not human-readable. Use it between your own services, for storage, or for IoT links,
and keep JSON for public APIs and debugging.

### The functions

```c
size_t len;
uint8_t* bin = nyxj_msgpack_encode(doc, &len);                 /* tree -> bytes (free with free()) */
nyxj_value* back = nyxj_msgpack_decode(bin, len, &err);        /* bytes -> tree (nyxj_free_value) */

uint8_t* bin2 = nyxj_json_to_msgpack(json_text, json_len, &len, &err);  /* text -> bytes */
char* text = nyxj_msgpack_to_json(bin, len, &err);                       /* bytes -> text */
```

When decoding MessagePack produced by other software, binary payloads (the `bin` type, which JSON does
not have) become strings.

### Example

```c
#include "nyxj.h"
#include "nyxj_msgpack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char* json = "{\"device\": \"probe-7\", \"readings\": [21.5, 21.7, 22.0], \"ok\": true}";
    nyxj_result r = nyxj_parse(json);
    if (!r.is_valid) return 1;

    size_t bin_len;
    uint8_t* bin = nyxj_msgpack_encode(r.root, &bin_len);
    printf("JSON text: %zu bytes, MessagePack: %zu bytes\n", strlen(json), bin_len);

    nyxj_error err;
    nyxj_value* back = nyxj_msgpack_decode(bin, bin_len, &err);
    printf("round trip identical: %s\n", nyxj_equals(r.root, back) ? "yes" : "no");

    char* text = nyxj_msgpack_to_json(bin, bin_len, &err);
    printf("%s\n", text);

    free(text);
    nyxj_free_value(back);
    free(bin);
    nyxj_free_value(r.root);
    return 0;
}
```

```text
JSON text: 65 bytes, MessagePack: 45 bytes
round trip identical: yes
{"device":"probe-7","readings":[21.5,21.7,22],"ok":true}
```

### Recap

- MessagePack is "binary JSON": same data, smaller and faster, not human-readable.
- `nyxj_msgpack_encode` / `decode` work on trees; `nyxj_json_to_msgpack` / `nyxj_msgpack_to_json` on text.
- Ideal between your own services; keep JSON at public boundaries.

---

## Chapter 17. The Zero-Copy View

### The idea

Building a tree copies every string and allocates every value. If you only need three fields out of a
large document, most of that work is wasted. The **view** takes another approach: it validates the input
and records where each value starts and ends, in a flat array of **tokens** that point back into your
original buffer. No string is copied. With a token array you provide (on the stack, for example), parsing
performs **zero heap allocations**.

### Tokens

`nyxj tokens` (Chapter 4) shows them:

```text
#0    object  size=1    [0..13) {"a":[1,"x"]}
#1    string  size=0    [2..3) a
#2    array   size=2    [5..12) [1,"x"]
#3    number  size=0    [6..7) 1
#4    string  size=0    [9..10) x
```

Each `nyxj_tok` has:

| Field | Meaning |
|---|---|
| `type` | `NYXJ_TOK_OBJECT`, `ARRAY`, `STRING`, `NUMBER`, `TRUE`, `FALSE` or `NULL` |
| `start`, `end` | Byte range in the buffer (for strings, without the quotes) |
| `size` | Number of members (object) or items (array) |
| `next` | Index of the first token after this value and all its children |
| `escaped` | The string contains escape sequences such as `\n` or `\"` |

Tokens are stored in document order. In an object, each member is a key token followed by its value.

### The functions

```c
nyxj_tok tokens[256];
nyxj_view v;
bool ok = nyxj_view_parse(&v, buf, len, tokens, 256, &err);   /* your array: zero allocations */
bool ok2 = nyxj_view_parse(&v, buf, len, NULL, 0, &err);      /* NULL: heap array that grows */
nyxj_view_free(&v);                                           /* required with a heap array */
```

With your own array, a document that needs more tokens fails with `NYXJ_ERR_ELEMENT_LIMIT`.

Navigation functions take and return **token indexes**; "not found" is `NYXJ_VIEW_NONE`:

| Function | Purpose |
|---|---|
| `nyxj_view_path(&v, "user.roles[1]")` | Token at a dot path from the root |
| `nyxj_view_find(&v, object_tok, "key")` | Value token of a member |
| `nyxj_view_index(&v, array_tok, i)` | Token of the i-th item |
| `nyxj_view_eq(&v, tok, "text")` | Does this string equal "text"? |
| `nyxj_view_raw(&v, tok, &len)` | Pointer into the buffer and length (escapes not decoded) |
| `nyxj_view_string(&v, tok, out, size)` | Decodes the string into `out`, returns the decoded length |
| `nyxj_view_number(&v, tok)`, `nyxj_view_bool(&v, tok)` | Read scalars |
| `nyxj_view_to_value(&v, tok)` | Builds a normal tree for this part only (you free it) |

**Pitfall**: the view points into your buffer. Keep the buffer alive and unchanged as long as you use the
view.

### Example

```c
#include "nyxj.h"
#include "nyxj_view.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* buf = "{\"event\": \"login\", \"user\": {\"name\": \"Ada \\\"the first\\\"\", \"id\": 42},"
                      " \"roles\": [\"admin\", \"dev\"], \"ok\": true}";
    nyxj_tok tokens[64];   /* on the stack: no heap allocation at all */
    nyxj_view v;
    nyxj_error err;
    if (!nyxj_view_parse(&v, buf, strlen(buf), tokens, 64, &err)) {
        printf("error: %s\n", err.message);
        return 1;
    }
    printf("%zu tokens\n", v.count);

    if (nyxj_view_eq(&v, nyxj_view_path(&v, "event"), "login")) printf("this is a login event\n");

    size_t len;
    size_t name_tok = nyxj_view_path(&v, "user.name");
    const char* raw = nyxj_view_raw(&v, name_tok, &len);
    printf("raw name: %.*s\n", (int)len, raw);   /* points into buf: escapes are still there */
    char name[32];
    nyxj_view_string(&v, name_tok, name, sizeof name);
    printf("decoded name: %s\n", name);

    printf("id: %g, ok: %s\n", nyxj_view_number(&v, nyxj_view_path(&v, "user.id")),
           nyxj_view_bool(&v, nyxj_view_path(&v, "ok")) ? "true" : "false");

    size_t roles = nyxj_view_path(&v, "roles");
    for (size_t i = 0; i < v.tokens[roles].size; i++) {
        raw = nyxj_view_raw(&v, nyxj_view_index(&v, roles, i), &len);
        printf("role %zu: %.*s\n", i, (int)len, raw);
    }
    nyxj_view_free(&v);
    return 0;
}
```

```text
15 tokens
this is a login event
raw name: Ada \"the first\"
decoded name: Ada "the first"
id: 42, ok: true
role 0: admin
role 1: dev
```

### When to use the view

- You read a few fields from large documents (routing on a `type` field, extracting an id).
- You run on a device where heap allocation is forbidden or expensive.
- You want to validate and index JSON as fast as possible, and only materialize the parts you need with
  `nyxj_view_to_value`.

### Recap

- The view indexes the buffer into tokens without copying strings.
- With a caller-provided token array, parsing makes zero heap allocations.
- Navigate with paths, `find` and `index`; read with `raw`, `string`, `number`, `bool`.
- The buffer must outlive the view.

---

## Chapter 18. Streaming Huge Data: the SAX Parser

### Tree parsing versus event parsing

Everything so far loaded the whole document in memory. That does not work for a 20 GB log file, a socket
that never closes, or a sensor that sends readings forever.

A **SAX** (event-driven) parser reads the input piece by piece and calls your functions as it recognizes
each element: "an object starts", "a key named `celsius`", "the number 21.5", "the object ends". It never
builds a tree, so its memory use is **constant**: it is fixed when you create the parser and never grows,
whatever the size of the stream.

Think of the difference between photocopying a whole book before reading it (tree) and reading it aloud
page by page (events).

### The handler

You provide callbacks in a `nyxj_sax_handler`. Each one is optional and returns `true` to continue or
`false` to stop the parsing:

```c
typedef struct {
    bool (*on_object_start)(void* user);
    bool (*on_object_end)(void* user);
    bool (*on_array_start)(void* user);
    bool (*on_array_end)(void* user);
    bool (*on_key)(void* user, const char* key, size_t len);
    bool (*on_string)(void* user, const char* s, size_t len);
    bool (*on_number)(void* user, double value, const char* raw, size_t len);
    bool (*on_bool)(void* user, bool value);
    bool (*on_null)(void* user);
    bool (*on_document_end)(void* user);
} nyxj_sax_handler;
```

`user` is any pointer you choose: it is how your callbacks share state. Strings arrive already decoded,
and `on_number` also gives you the raw text of the number.

### Options

| Field | Default | Meaning |
|---|---|---|
| `max_depth` | 512 | Maximum nesting |
| `max_token_size` | 64 KiB | Longest string or number; sets the size of the internal buffer |
| `max_elements` | unlimited | Maximum items per container |
| `multi_document` | `false` | Accept a sequence of documents: NDJSON (one JSON per line) or concatenated JSON |

### Feeding data

```c
nyxj_sax* sax = nyxj_sax_create(&handler, &my_state, &options);
nyxj_sax_feed(sax, chunk1, len1);   /* any number of times, chunks of any size */
nyxj_sax_feed(sax, chunk2, len2);
nyxj_sax_finish(sax);               /* end of input: checks that nothing is left unfinished */
nyxj_sax_destroy(sax);
```

A token can be split between two chunks (`{"cel` then `sius": 21.5}`): the parser handles it. `feed` and
`finish` return `false` on error; `nyxj_sax_error(sax)` then describes it, with line and column.

Other helpers: `nyxj_sax_memory` (the constant footprint in bytes), `nyxj_sax_bytes` (bytes consumed),
`nyxj_sax_documents` (completed documents), `nyxj_sax_reset` (reuse the parser for a new stream), and
`nyxj_sax_validate(data, len, &opts, &err)` to check a buffer without building anything.

### Example: average temperature of an endless sensor feed

```c
#include "nyxj.h"
#include "nyxj_sax.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    bool next_is_celsius;   /* the last key was "celsius" */
    double sum;
    unsigned long count;
} stats;

static bool on_key(void* user, const char* key, size_t len) {
    stats* s = user;
    s->next_is_celsius = (len == 7 && memcmp(key, "celsius", 7) == 0);
    return true;
}

static bool on_number(void* user, double value, const char* raw, size_t len) {
    (void)raw;
    (void)len;
    stats* s = user;
    if (s->next_is_celsius) {
        s->sum += value;
        s->count++;
    }
    s->next_is_celsius = false;
    return true;
}

int main(int argc, char** argv) {
    FILE* f = argc > 1 ? fopen(argv[1], "rb") : stdin;
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    stats s = {0};
    nyxj_sax_handler h = {0};
    h.on_key = on_key;
    h.on_number = on_number;
    nyxj_sax_options opts = {0};
    opts.multi_document = true;   /* NDJSON: one document per line */
    nyxj_sax* sax = nyxj_sax_create(&h, &s, &opts);

    char chunk[4096];
    size_t n;
    bool ok = true;
    while (ok && (n = fread(chunk, 1, sizeof chunk, f)) > 0) ok = nyxj_sax_feed(sax, chunk, n);
    if (ok) ok = nyxj_sax_finish(sax);
    if (!ok)
        printf("error: %s\n", nyxj_sax_error(sax)->message);
    else
        printf("%llu documents, average %.2f C, parser memory %zu bytes\n",
               (unsigned long long)nyxj_sax_documents(sax), s.count ? s.sum / s.count : 0.0,
               nyxj_sax_memory(sax));
    nyxj_sax_destroy(sax);
    if (f != stdin) fclose(f);
    return ok ? 0 : 1;
}
```

Generate a million readings and stream them:

```bash
awk 'BEGIN { for (i = 0; i < 1000000; i++) printf "{\"sensor\":\"t%d\",\"celsius\":%.1f}\n", i % 8, 20 + (i % 100) / 10 }' > feed.ndjson
./sensors feed.ndjson
```

```text
1000000 documents, average 24.95 C, parser memory 68521 bytes
```

The memory reported is the same for a thousand lines or a billion: only the speed of your disk matters.

### Recap

- SAX parsing calls your functions for each element instead of building a tree.
- Memory is constant: ideal for huge files, sockets and endless streams.
- Feed chunks of any size, then `finish`; enable `multi_document` for NDJSON.

---

## Chapter 19. Repairing Broken JSON

### When repair makes sense

Sometimes rejecting input is not an option: a configuration file edited by hand, the answer of an AI
model that wrapped JSON in Markdown, a legacy system that writes Python dictionaries. NyxJ can turn such
"almost-JSON" into valid JSON.

**Pitfall**: repair is a **guess**. Use it where a human or a tolerant pipeline expects it, then
validate the result (with a schema, Chapter 13). Never silently repair financial or security-sensitive
input.

### What it fixes

| Fix (`NYXJ_FIX_*`) | Example of input | Becomes |
|---|---|---|
| `COMMENT` | `// note`, `/* note */` | removed |
| `QUOTES` | `'text'` | `"text"` |
| `UNQUOTED_KEY` | `{name: 1}` | `{"name": 1}` |
| `TRAILING_COMMA` | `[1, 2,]` | `[1, 2]` |
| `MISSING_COMMA` | `[1 2]` | `[1, 2]` |
| `MISSING_COLON` | `{"a" 1}` | `{"a": 1}` |
| `UNCLOSED` | `{"a": [1, 2` | `{"a": [1, 2]}` |
| `LITERAL` | `True`, `None`, `undefined`, `NaN` | `true`, `null`, `null`, `null` |
| `NUMBER` | `+1`, `.5`, `007`, `0x1F` | `1`, `0.5`, `7`, `31` |
| `ESCAPE` | raw control characters, bad escapes | escaped properly |
| `BRACKET` | `[1, 2}` | `[1, 2]` |
| `CODE_FENCE` | ` ```json ... ``` ` | the JSON inside |
| `BOM` | a leading UTF-8 byte order mark | removed |
| `MISSING_VALUE`, `TRAILING_DATA`, `GARBAGE` | `{"a": }`, `{} xyz`, stray characters | a `null`, the extra text removed |

### The functions

```c
nyxj_repair_report rep;
size_t out_len;
char* fixed = nyxj_repair(text, len, &out_len, &rep);    /* compact valid JSON (free it), or NULL */
nyxj_result r = nyxj_parse_lenient(text, len, &rep);     /* tries a strict parse first, repairs if needed */
```

The report counts the corrections (`rep.fixes`) and records their kinds as a bitmask (`rep.kinds`).
`nyxj_repair_kind_name(NYXJ_FIX_QUOTES)` gives `"quotes"`, and `nyxj_repair_report_to_value(&rep)` gives
`{"fixes": N, "kinds": [...]}`.

### Example: cleaning up an AI model's answer

````c
#include "nyxj.h"
#include "nyxj_repair.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char* messy =
        "```json\n"
        "{\n"
        "  name: 'Ada',      // unquoted key, single quotes\n"
        "  active: True,     /* Python literal */\n"
        "  score: .5,\n"
        "  tags: ['c', 'go',],\n"
        "  extra: None\n";   /* the closing brace and the fence are missing */

    nyxj_repair_report rep;
    size_t len;
    char* fixed = nyxj_repair(messy, strlen(messy), &len, &rep);
    if (!fixed) {
        printf("nothing salvageable\n");
        return 1;
    }
    printf("%s\n%d fix(es):", fixed, rep.fixes);
    for (unsigned bit = 0; bit < NYXJ_FIX_KIND_COUNT; bit++)
        if (rep.kinds & (1u << bit)) printf(" %s", nyxj_repair_kind_name(1u << bit));
    printf("\n");
    free(fixed);

    nyxj_result r = nyxj_parse_lenient("{\"ok\": true}", 12, &rep);
    printf("valid input: %d fix(es)\n", rep.fixes);
    nyxj_free_value(r.root);
    return 0;
}
````

```text
{"name":"Ada","active":true,"score":0.5,"tags":["c","go"],"extra":null}
16 fix(es): comment quotes unquoted_key trailing_comma unclosed literal number code_fence
valid input: 0 fix(es)
```

### Recap

- `nyxj_repair` turns almost-JSON into valid JSON and reports what it changed.
- `nyxj_parse_lenient` parses strictly first and only repairs when needed.
- Repair is a guess: validate the result, and never repair sensitive input silently.

---

## Chapter 20. Security: Limits and the Guardian

### The threat

Any data that crosses a network can be hostile. Attackers send documents that are technically valid but
designed to exhaust your stack, memory or CPU (JSON denial of service), malformed bytes that confuse
later processing, or oversized payloads. NyxJ gives you several layers of defense; use all of them.

```text
 request ──► 1. size limit ──► 2. Guardian ──► 3. parser limits ──► 4. schema ──► 5. your checks
            (max_body_size)   (nyxj_gate)      (parse options)     (Chapter 13)    (business rules)
```

1. **Size limit**: the web framework rejects bodies above `max_body_size` with `413` before reading them
   entirely (Chapter 26).
2. **The Guardian** inspects the raw bytes in one pass, without allocating, before any parser runs.
3. **Parser limits** (Chapter 7) bound depth, size, strings and elements during parsing.
4. **Schema validation** checks the shape and ranges of the data.
5. **Your own checks**: permissions, business rules.

### The Guardian

`nyxj_gate_check` verifies, in a single pass with constant memory:

- the total size;
- that the bytes are valid UTF-8, with no NUL bytes or raw control characters in strings;
- the nesting depth, string and number lengths, and element counts;
- that the JSON is well formed and, optionally, that the root is an object;
- it also skips a leading UTF-8 byte order mark (BOM) and tells you where the payload really starts.

```c
nyxj_gate_policy policy = nyxj_gate_policy_strict();
nyxj_gate_verdict verdict;
if (!nyxj_gate_check(body, len, &policy, &verdict)) {
    /* verdict.reason: short code; verdict.error: full error with position */
}
```

Two ready-made policies:

| Policy | Max size | Max depth | Max string | Max elements | Root must be an object |
|---|---|---|---|---|---|
| `nyxj_gate_policy_default()` | 1 MiB | 64 | 64 KiB | 10 000 | no |
| `nyxj_gate_policy_strict()` | 64 KiB | 16 | 4 KiB | 1 000 | yes |

Start from one of them and adjust the fields (`max_size`, `max_depth`, `max_string_length`,
`max_elements`, `require_object_root`, `allow_empty`).

The verdict's `reason` is one of: `ok`, `size_limit`, `depth_limit`, `string_limit`, `element_limit`,
`invalid_utf8`, `control_char`, `root_type`, `empty_or_truncated`, `malformed`. The Safe-Gate proxy
(Chapter 31) maps them to HTTP statuses: `413` for `size_limit`, `400` for `malformed` and
`empty_or_truncated`, `422` for the others.

### Example

```c
#include "nyxj.h"
#include "nyxj_gate.h"
#include <stdio.h>
#include <string.h>

static void inspect(const char* label, const char* body, const nyxj_gate_policy* p) {
    nyxj_gate_verdict v;
    if (nyxj_gate_check(body, strlen(body), p, &v))
        printf("%-8s allowed\n", label);
    else
        printf("%-8s blocked: %s (%s)\n", label, v.reason, v.error.message);
}

int main(void) {
    nyxj_gate_policy strict = nyxj_gate_policy_strict();
    inspect("normal", "{\"user\": \"ada\", \"amount\": 42}", &strict);
    inspect("bomb", "{\"a\": [[[[[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]]]]]}", &strict);
    inspect("array", "[1, 2, 3]", &strict);
    inspect("broken", "{\"user\": \"ada\"", &strict);
    inspect("utf8", "{\"name\": \"\xff\xfe\"}", &strict);

    nyxj_gate_policy small = nyxj_gate_policy_default();
    small.max_size = 16;
    inspect("big", "{\"comment\": \"longer than sixteen bytes\"}", &small);
    return 0;
}
```

```text
normal   allowed
bomb     blocked: depth_limit (maximum nesting depth exceeded at line 1, column 22)
array    blocked: root_type (payload root must be a JSON object)
broken   blocked: empty_or_truncated (unexpected end of input at line 1, column 15)
utf8     blocked: invalid_utf8 (invalid UTF-8 byte sequence at line 1, column 11)
big      blocked: size_limit (payload exceeds the maximum size)
```

### Security checklist

- Set parse limits (or a Guardian policy) on every input that comes from outside.
- Reject duplicate keys for security-sensitive data (`NYXJ_PARSE_REJECT_DUPLICATE_KEYS`).
- Validate shapes with a schema before using the data.
- Serve over HTTPS (Chapter 26) and verify certificates when calling other services (Chapter 27).
- Never log secrets (tokens, passwords) or whole request bodies containing personal data.
- Run your tests under AddressSanitizer and UndefinedBehaviorSanitizer (`make SAN=1 test`).

### Recap

- Defend in layers: size limit, Guardian, parser limits, schema, business checks.
- `nyxj_gate_check` inspects raw bytes in one pass with no allocation.
- Use the default or strict policy as a starting point; `reason` explains every rejection.

---

## Chapter 21. Reliability for Critical Systems

In medical devices, industrial controllers or vehicles, a single flipped bit in memory (caused by a
faulty chip, electrical noise or even cosmic rays) must not turn a dose of 5 mg into 500 mg. NyxJ offers
three tools borrowed from safety engineering.

### Structural checksums

```c
uint64_t sum = nyxj_checksum(doc);
```

A 64-bit FNV-1a fingerprint of the tree's structure and content. Two equal trees with the same member
order have the same checksum; any change gives (with overwhelming probability) a different one. Store it
next to a document and recompute it before use to detect corruption. It is fast but **not
cryptographic**: do not use it to protect against a deliberate attacker.

### Shadow execution

```c
nyxj_result r = nyxj_parse_verified(json, len, &opts);   /* opts may be NULL */
```

The input is parsed twice (on two threads when available) and both trees are compared. If they differ,
something corrupted one of them, and the result is an error with code `NYXJ_ERR_INTEGRITY` instead of
silently wrong data. It costs about twice the CPU of a normal parse.

### Triple modular redundancy (TMR)

```c
nyxj_tmr* tmr = nyxj_tmr_create(doc);                 /* keeps three independent copies */
const nyxj_value* safe = nyxj_tmr_read(tmr, &repaired); /* majority vote; NULL if no two copies agree */
nyxj_tmr_write(tmr, new_doc);                         /* updates all three copies */
nyxj_tmr_free(tmr);
```

On every read, the three copies vote. If one differs from the two others, it is outvoted **and repaired**
in place; `repaired` tells you it happened and `tmr->repairs` counts repairs over time.

### Example

```c
#include "nyxj.h"
#include "nyxj_reliability.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char* json = "{\"pump\": \"P-12\", \"dose_mg\": 5, \"max_rate\": 2.5}";

    nyxj_result r = nyxj_parse_verified(json, strlen(json), NULL);
    if (!r.is_valid) {
        printf("integrity problem: %s\n", r.error.message);
        return 1;
    }
    printf("checksum: %016llx\n", (unsigned long long)nyxj_checksum(r.root));

    nyxj_tmr* tmr = nyxj_tmr_create(r.root);
    nyxj_object_set_number(tmr->replica[1], "dose_mg", 500);   /* simulate a memory corruption */

    bool repaired;
    const nyxj_value* safe = nyxj_tmr_read(tmr, &repaired);
    printf("dose: %g mg, repaired: %s, total repairs: %llu\n", nyxj_get_number(safe, "dose_mg"),
           repaired ? "yes" : "no", (unsigned long long)tmr->repairs);
    printf("replica 1 restored: %s\n", nyxj_equals(tmr->replica[1], r.root) ? "yes" : "no");

    nyxj_tmr_free(tmr);
    nyxj_free_value(r.root);
    return 0;
}
```

```text
checksum: 301823be09f6196b
dose: 5 mg, repaired: yes, total repairs: 1
replica 1 restored: yes
```

### Recap

- `nyxj_checksum`: fast structural fingerprint to detect corruption (not a security hash).
- `nyxj_parse_verified`: parse twice and compare, `NYXJ_ERR_INTEGRITY` on mismatch.
- `nyxj_tmr_*`: three copies, majority vote and automatic repair.

---

# Part IV. Runtime Services

## Chapter 22. Concurrency: the Thread Pool

### Why threads?

A modern CPU has several cores. A program that does everything in one thread uses only one of them, and
a slow task (parsing a 50 MB upload) blocks everything behind it. A **thread pool** keeps a fixed number
of **worker threads** ready; you hand them **tasks** (a function and an argument) through a **queue**, and
they run them in parallel.

```text
 submit(task) ──► [ queue: t5 t4 t3 ] ──► worker 1 runs t1
                                      ──► worker 2 runs t2
```

### The functions

```c
nyxj_pool* pool = nyxj_pool_create(threads, queue_capacity);  /* threads <= 0: one per CPU */
bool queued = nyxj_pool_submit(pool, function, argument);     /* false when the queue is full */
nyxj_pool_wait(pool);                                         /* blocks until every task is done */
nyxj_pool_set_concurrency(pool, 2);                           /* at most 2 workers active (0 = all) */
nyxj_pool_stats_get(pool, &stats);                            /* threads, queued, active, completed, rejected */
nyxj_pool_destroy(pool);                                      /* finishes queued tasks, then stops */
```

A task is any function of the form `void task(void* arg)`.

### Backpressure

The queue has a fixed capacity on purpose. When producers are faster than workers, an unbounded queue
would grow until memory runs out. Instead, `nyxj_pool_submit` returns `false`: the queue is full, and you
decide what to do (wait, answer "503 Service Unavailable", drop the task, or run it yourself). This
principle is called **backpressure**. The web framework uses it to answer `503` when it is overloaded.

### Thread-safety rules

Tasks run at the same time, so:

- the NyxJ parser and serializer can be called from any number of threads;
- a tree must not be modified by one thread while another reads it (give each task its own data, or use
  a lock);
- the key-value store (Chapter 23) and metrics (Chapter 24) are thread-safe and can be shared freely;
- for your own shared variables, use C11 atomics (`<stdatomic.h>`) or a mutex.

### Example: processing orders in parallel

```c
#include "nyxj.h"
#include "nyxj_pool.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

static atomic_long total;   /* shared by all workers: atomic, no lock needed */

static void handle_order(void* arg) {
    int id = (int)(intptr_t)arg;
    char json[64];
    snprintf(json, sizeof json, "{\"id\": %d, \"amount\": %d}", id, id * 10);
    nyxj_result r = nyxj_parse(json);   /* safe to call from many threads at once */
    if (r.is_valid) atomic_fetch_add(&total, (long)nyxj_get_int(r.root, "amount"));
    nyxj_free_value(r.root);
}

int main(void) {
    nyxj_pool* pool = nyxj_pool_create(0, 256);   /* one worker per CPU, at most 256 queued tasks */
    int full = 0;
    for (int i = 1; i <= 1000; i++) {
        while (!nyxj_pool_submit(pool, handle_order, (void*)(intptr_t)i)) {
            full++;                 /* backpressure: let the workers catch up, then retry */
            nyxj_pool_wait(pool);
        }
    }
    nyxj_pool_wait(pool);
    nyxj_pool_stats st;
    nyxj_pool_stats_get(pool, &st);
    printf("%d workers, %llu tasks, total amount %ld (queue was full %d times)\n", st.threads,
           (unsigned long long)st.completed, atomic_load(&total), full);
    nyxj_pool_destroy(pool);
    return 0;
}
```

```text
4 workers, 1000 tasks, total amount 5005000 (queue was full 3 times)
```

The total is always `5005000`; the number of workers and how often the queue filled up depend on your
machine.

### Recap

- A pool runs tasks on a fixed set of worker threads fed by a bounded queue.
- A full queue makes `submit` return `false`: that is backpressure, handle it.
- Parsing is thread-safe; shared trees and your own variables need care.

---

## Chapter 23. The Embedded Key-Value Store

### What it is

`nyxj_kv` is a small in-memory database inside your process, in the spirit of Redis: it maps string keys
to JSON values, with optional expiration (TTL), atomic counters, prefix listing and snapshots on disk.
It is thread-safe. Typical uses: sessions, caches, rate-limit counters, feature flags, configuration
shared by request handlers, all without running an external database.

### The functions

| Function | Purpose |
|---|---|
| `nyxj_kv_create(capacity)` / `nyxj_kv_destroy(kv)` | Create (0 = default capacity) and destroy |
| `nyxj_kv_set(kv, key, value, ttl_ms)` | Store a **copy** of `value`; `ttl_ms` = 0 means forever |
| `nyxj_kv_get(kv, key)` | Returns a **copy** (you free it) or `NULL` |
| `nyxj_kv_del`, `nyxj_kv_exists` | Delete, test |
| `nyxj_kv_incr(kv, key, delta, &result)` | Atomic add; a missing key starts at 0; fails if the value is not a number |
| `nyxj_kv_expire(kv, key, ttl_ms)` | Set or change a TTL |
| `nyxj_kv_ttl(kv, key)` | Milliseconds left, `-1` if no TTL, `-2` if the key does not exist |
| `nyxj_kv_keys(kv, prefix)` | Sorted array of matching keys (you free it) |
| `nyxj_kv_count`, `nyxj_kv_sweep`, `nyxj_kv_clear` | Count, purge expired keys, remove everything |
| `nyxj_kv_save(kv, path, &err)` / `nyxj_kv_load(kv, path, &err)` | JSON snapshot on disk |
| `nyxj_kv_stats_get(kv, &stats)` | Keys, hits, misses, expirations, writes |

**Why copies?** Several threads may use the store at once. If `get` returned a pointer inside the store,
another thread could delete the value while you read it. Copies make every operation safe.

Expired keys disappear as soon as they are accessed, and `nyxj_kv_sweep` removes the others (the web
framework calls it periodically for you).

### Example

```c
#include "nyxj.h"
#include "nyxj_kv.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    nyxj_kv* kv = nyxj_kv_create(0);

    /* A session that expires after 30 minutes */
    nyxj_value* session = nyxj_new_object();
    nyxj_object_set_string(session, "user", "ada");
    nyxj_object_set_string(session, "role", "admin");
    nyxj_kv_set(kv, "session:abc123", session, 30 * 60 * 1000);
    nyxj_free_value(session);   /* the store keeps its own copy */

    nyxj_value* s = nyxj_kv_get(kv, "session:abc123");
    printf("user: %s, expires in about %lld min\n", nyxj_get_string(s, "user"),
           (long long)(nyxj_kv_ttl(kv, "session:abc123") + 59999) / 60000);
    nyxj_free_value(s);         /* get returns a copy too */

    /* A counter, for example to limit requests per client */
    double hits = 0;
    for (int i = 0; i < 3; i++) nyxj_kv_incr(kv, "hits:10.0.0.7", 1, &hits);
    nyxj_kv_expire(kv, "hits:10.0.0.7", 60 * 1000);   /* the window resets after a minute */
    printf("hits: %.0f\n", hits);

    nyxj_value* keys = nyxj_kv_keys(kv, "session:");
    char* text = nyxj_serialize(keys);
    printf("sessions: %s, total keys: %zu\n", text, nyxj_kv_count(kv));
    free(text);
    nyxj_free_value(keys);

    nyxj_error err;
    if (!nyxj_kv_save(kv, "store.json", &err)) printf("save failed: %s\n", err.message);
    nyxj_kv_destroy(kv);

    nyxj_kv* restored = nyxj_kv_create(0);
    if (nyxj_kv_load(restored, "store.json", &err))
        printf("restored %zu keys from store.json\n", nyxj_kv_count(restored));
    nyxj_kv_destroy(restored);
    return 0;
}
```

```text
user: ada, expires in about 30 min
hits: 3
sessions: ["session:abc123"], total keys: 2
restored 2 keys from store.json
```

The snapshot is plain JSON, `{"key": {"value": ..., "ttl_ms": N}, ...}`, so you can read or edit it with
any tool. TTLs are saved as time remaining.

### Recap

- An in-process, thread-safe store of JSON values with TTL, counters and prefix listing.
- `set` and `get` work on copies: always free what `get` returns.
- Snapshots (`save`/`load`) persist the store as readable JSON.

---

## Chapter 24. Metrics and System Monitoring

### Why metrics?

When a service runs in production, you need numbers: how many requests per second, how many errors, how
long they take, how much memory is used. **Prometheus** is the most common tool for this: it regularly
fetches ("scrapes") a page of metrics from each service and stores them, and tools like Grafana draw
graphs and raise alerts from them. NyxJ produces metrics in the Prometheus text format.

### The three metric types

| Type | Behavior | Examples |
|---|---|---|
| **Counter** | Only goes up | Requests served, errors, bytes processed |
| **Gauge** | Goes up and down | Queue length, temperature, open connections |
| **Histogram** | Counts observations in buckets, plus their sum and count | Request durations, payload sizes |

### Labels

A metric can be split by **labels**: `http_requests_total{method="GET",status="200"}`. In NyxJ you pass
the label body as a string, without braces: `"method=\"GET\",status=\"200\""`. Use
`nyxj_metrics_escape_label` to put arbitrary text in a label value safely.

**Pitfall**: every distinct combination of labels is a separate series stored forever. Never use
unbounded values (user ids, full URLs, timestamps) as labels.

### The functions

```c
nyxj_metrics* m = nyxj_metrics_create();
nyxj_metric* c = nyxj_metrics_counter(m, "name", "help text", "label=\"value\"");  /* get or create */
nyxj_metric* g = nyxj_metrics_gauge(m, "name", "help", NULL);
nyxj_metric* h = nyxj_metrics_histogram(m, "name", "help", NULL, buckets, nbuckets);  /* NULL: latency buckets */

nyxj_metric_inc(c);  nyxj_metric_add(c, 5);   /* counters (negative additions are ignored) */
nyxj_metric_set(g, 42);                       /* gauges */
nyxj_metric_observe(h, 0.120);                /* histograms */
double v = nyxj_metric_value(c);

nyxj_metrics_add_collector(m, refresh_fn, user);  /* called just before each rendering */
char* text = nyxj_metrics_render(m);              /* Prometheus text format (you free it) */
```

Asking for the same name and labels again returns the same series, so you can look a metric up where you
use it. Every function is thread-safe. Collectors are the place to refresh gauges lazily (read a queue
length only when someone asks).

Conventions: name metrics `<app>_<what>_<unit>`, use `_total` for counters and base units (seconds,
bytes).

### System statistics

```c
nyxj_sys_stats st;
nyxj_sys_stats_get(&st);   /* cpu_seconds, rss_bytes, load1, cpu_temp_c, cpu_count */
```

`load1` (load average) and `cpu_temp_c` are `-1` when the platform does not provide them (load is not
available on Windows; temperature comes from Linux thermal zones).

### Example

```c
#include "nyxj.h"
#include "nyxj_metrics.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    nyxj_metrics* m = nyxj_metrics_create();

    nyxj_metric* eu = nyxj_metrics_counter(m, "shop_orders_total", "Orders received", "region=\"eu\"");
    nyxj_metric* us = nyxj_metrics_counter(m, "shop_orders_total", "Orders received", "region=\"us\"");
    nyxj_metric_inc(eu);
    nyxj_metric_add(eu, 2);
    nyxj_metric_inc(us);

    nyxj_metric_set(nyxj_metrics_gauge(m, "shop_cart_items", "Items in open carts", NULL), 17);

    const double buckets[] = {0.01, 0.1, 1};
    nyxj_metric* lat = nyxj_metrics_histogram(m, "shop_checkout_seconds", "Checkout duration", NULL, buckets, 3);
    nyxj_metric_observe(lat, 0.004);
    nyxj_metric_observe(lat, 0.25);

    char* text = nyxj_metrics_render(m);
    printf("%s", text);
    free(text);

    nyxj_sys_stats st;
    if (nyxj_sys_stats_get(&st))
        printf("\nprocess: %.2f s of CPU, %llu bytes resident, %d CPUs\n", st.cpu_seconds,
               (unsigned long long)st.rss_bytes, st.cpu_count);
    nyxj_metrics_destroy(m);
    return 0;
}
```

```text
# HELP shop_orders_total Orders received
# TYPE shop_orders_total counter
shop_orders_total{region="eu"} 3
shop_orders_total{region="us"} 1
# HELP shop_cart_items Items in open carts
# TYPE shop_cart_items gauge
shop_cart_items 17
# HELP shop_checkout_seconds Checkout duration
# TYPE shop_checkout_seconds histogram
shop_checkout_seconds_bucket{le="0.01"} 1
shop_checkout_seconds_bucket{le="0.1"} 1
shop_checkout_seconds_bucket{le="1"} 2
shop_checkout_seconds_bucket{le="+Inf"} 2
shop_checkout_seconds_sum 0.254
shop_checkout_seconds_count 2

process: 0.02 s of CPU, 4456448 bytes resident, 4 CPUs
```

Histogram buckets are cumulative: `le="1"` counts every observation up to 1 second, including the
faster ones.

In a web service you do not create your own registry: use the application's with
`nyxj_app_metrics(app)`, and your metrics appear on `/metrics` next to the built-in ones (Chapter 28).

### Recap

- Counters go up, gauges go up and down, histograms measure distributions.
- Labels split a metric; keep their values bounded.
- `nyxj_metrics_render` produces the Prometheus text format; `nyxj_sys_stats_get` reads process and
  system resources.

---

## Chapter 25. Inter-Process Communication

When several NyxJ services run on the **same machine**, going through the TCP/IP network stack is
unnecessary overhead. NyxJ offers two faster channels.

### Unix domain sockets

A Unix domain socket looks like a network connection but is addressed by a file path and never leaves
the machine. NyxJ sends **frames** (each message is prefixed by its length, so messages never get mixed
up) and provides JSON helpers. They work on Linux, macOS and Windows 10 (version 1803) or later.

| Function | Purpose |
|---|---|
| `nyxj_ipc_listen(path, &err)` | Server: create the socket file and listen (a stale file is replaced) |
| `nyxj_ipc_accept(listener, timeout_ms, &err)` | Server: wait for a client (`-1` = forever) |
| `nyxj_ipc_connect(path, &err)` | Client: connect |
| `nyxj_ipc_send` / `nyxj_ipc_recv` | Send and receive raw frames (`recv` returns a buffer you free) |
| `nyxj_ipc_send_json` / `nyxj_ipc_recv_json` | Same with JSON values |
| `nyxj_ipc_close(ch)` | Close; closing the listener also removes the socket file |

A server, `ipc_server.c`:

```c
#include "nyxj.h"
#include "nyxj_ipc.h"
#include <stdio.h>

int main(void) {
    nyxj_error err;
    nyxj_ipc* listener = nyxj_ipc_listen("nyxj-demo.sock", &err);
    if (!listener) {
        printf("listen failed: %s\n", err.message);
        return 1;
    }
    printf("waiting for a client...\n");
    nyxj_ipc* ch = nyxj_ipc_accept(listener, -1, &err);
    if (ch) {
        nyxj_value* msg = nyxj_ipc_recv_json(ch, 5000, &err);   /* 5 second timeout */
        if (msg) {
            printf("received: %s\n", nyxj_get_string(msg, "msg"));
            nyxj_object_set_bool(msg, "pong", true);
            nyxj_ipc_send_json(ch, msg, &err);
            nyxj_free_value(msg);
        }
        nyxj_ipc_close(ch);
    }
    nyxj_ipc_close(listener);
    return 0;
}
```

A client, `ipc_client.c`:

```c
#include "nyxj.h"
#include "nyxj_ipc.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    nyxj_error err;
    nyxj_ipc* ch = nyxj_ipc_connect("nyxj-demo.sock", &err);
    if (!ch) {
        printf("connect failed: %s\n", err.message);
        return 1;
    }
    nyxj_value* ping = nyxj_new_object();
    nyxj_object_set_string(ping, "msg", "ping");
    nyxj_ipc_send_json(ch, ping, &err);
    nyxj_free_value(ping);

    nyxj_value* reply = nyxj_ipc_recv_json(ch, 5000, &err);
    if (reply) {
        char* text = nyxj_serialize(reply);
        printf("reply: %s\n", text);
        free(text);
        nyxj_free_value(reply);
    }
    nyxj_ipc_close(ch);
    return 0;
}
```

Run the server in one terminal and the client in another: the client prints
`reply: {"msg":"ping","pong":true}`.

### Shared memory ring buffer

For the lowest possible latency, two processes can share a region of memory. NyxJ organizes it as a
**ring buffer** with one producer and one consumer, synchronized with atomic operations instead of locks.

```c
/* Producer process: creates and owns the ring */
nyxj_shm* ring = nyxj_shm_create("sensor-feed", 1 << 20, &err);   /* 1 MiB */
if (!nyxj_shm_write(ring, msg, strlen(msg))) {
    /* full: the consumer is behind, retry later or drop */
}
nyxj_shm_close(ring);   /* the creator also removes the shared segment */

/* Consumer process: opens the existing ring */
nyxj_shm* in = nyxj_shm_open("sensor-feed", &err);
size_t len;
char* m;
while ((m = nyxj_shm_read(in, &len)) != NULL) {   /* NULL when empty */
    handle(m, len);
    free(m);                                       /* each message is a NUL-terminated copy */
}
nyxj_shm_close(in);
```

Rules: exactly one writer and one reader per ring; the producer must create the ring before the consumer
opens it, and keep it open while the consumer uses it.

### Which one to choose?

| | Unix socket | Shared memory ring |
|---|---|---|
| Direction | Both ways | One way (producer to consumer) |
| Latency | Very low | Lowest |
| Programs | Many clients per server | Exactly one producer and one consumer |
| Best for | Request and response between services | High-rate feeds (sensors, market data, logs) |

### Recap

- Unix sockets: framed messages and JSON helpers between processes on one machine.
- Shared memory ring: one producer, one consumer, lock-free, lowest latency.
- Both avoid the TCP/IP stack entirely.

---

# Part V. Web and Network

## Chapter 26. Building a Web Service

This is where the pieces come together: NyxJ's web framework (`nyxj_http.h`) lets you write an HTTP API
in C with the comfort of frameworks like Express (Node.js) or Flask (Python).

### HTTP in one minute

A client (browser, mobile app, another service, `curl`) sends a **request**; the server answers with a
**response**.

- A request has a **method** (`GET` to read, `POST` to create, `PUT` to replace, `PATCH` to modify,
  `DELETE` to remove), a **path** (`/todos/42`), an optional **query string** (`?limit=10`),
  **headers** (`Content-Type: application/json`) and an optional **body**.
- A response has a **status code**, headers and a body. The first digit of the status tells the story:
  `2xx` success (`200 OK`, `201 Created`, `204 No Content`), `4xx` client error (`400 Bad Request`,
  `401 Unauthorized`, `404 Not Found`, `413 Payload Too Large`, `422 Unprocessable`), `5xx` server
  error (`500`, `503 Service Unavailable`).

### The smallest service

```c
#include "nyxj_http.h"

static void hello(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = nyxj_new_object();
    nyxj_object_set_string(body, "hello", nyxj_req_param(req, "name"));
    nyxj_res_json(res, 200, body);   /* takes ownership of body */
}

int main(void) {
    nyxj_app* app = nyxj_app_create(NULL);   /* NULL: default configuration, port 8000 */
    nyxj_app_get(app, "/hello/:name", hello, NULL);
    return nyxj_app_run(app);                /* serves until stopped */
}
```

Compile it like any NyxJ program (Chapter 5), run it, then open `http://localhost:8000/hello/Ada`:

```json
{"hello":"Ada"}
```

You also get, for free: `/health`, `/ready`, `/metrics`, `/openapi.json` and interactive documentation at
`/docs`.

### Configuration

```c
nyxj_app_config cfg = nyxj_app_config_default();
cfg.listen_url = "http://0.0.0.0:8080";
cfg.threads = 4;
nyxj_app* app = nyxj_app_create(&cfg);
```

| Field | Default | Meaning |
|---|---|---|
| `listen_url` | `http://0.0.0.0:8000` | Address and port; `0.0.0.0` means every network interface, port `0` picks a free port |
| `threads` | `0` | Worker pool size: `0` = one per CPU, `-1` = no pool |
| `queue_capacity` | `1024` | Pending async jobs before answering `503` |
| `max_body_size` | 16 MiB | Larger bodies are rejected with `413` |
| `parse` | defaults | Parse options (limits) applied to request bodies |
| `tls_cert_file`, `tls_key_file` | `NULL` | PEM files: enable HTTPS |
| `cors` | `true` | Add CORS headers and answer browser preflight (`OPTIONS`) requests |
| `access_log` | `true` | One log line per request |
| `enable_metrics`, `enable_health`, `enable_docs` | `true` | Built-in endpoints |
| `title`, `description`, `service_version` | | Shown in the OpenAPI document |
| `ready_queue_ratio` | `0.9` | `/ready` answers `503` above this queue fill ratio |
| `heartbeat_ms`, `heartbeat_url` | `5000`, `NULL` | Resource sampling period and optional webhook (Chapter 28) |
| `eco_temp_c`, `eco_load` | `0` (off) | Eco-Throttle thresholds (Chapter 28) |

### Routes

```c
nyxj_app_get(app, "/todos", list_todos, NULL);
nyxj_app_post(app, "/todos", create_todo, NULL);
nyxj_app_put(app, "/todos/:id", replace_todo, NULL);
nyxj_app_delete(app, "/todos/:id", delete_todo, NULL);
nyxj_app_route(app, "PATCH", "/todos/:id", update_todo, NULL);   /* any other method */
```

Patterns:

| Pattern | Matches | Parameters |
|---|---|---|
| `/health` | `/health` and `/health/` | none |
| `/users/:id` | `/users/42` (not `/users/` nor `/users/42/x`) | `id` = `"42"` |
| `/users/:id/posts/:post` | `/users/42/posts/7` | `id`, `post` |
| `/static/*` | `/static/css/app.css` | `*` = `"css/app.css"` |

The last argument of each route is a `void*` **user data** pointer, available in the handler as
`req->user`: a handy way to pass configuration or shared state without globals.

Routes are tried in registration order and the first match wins. When the path matches but not the
method, the client gets `405 Method Not Allowed` with an `Allow` header; when nothing matches, `404`.
`nyxj_app_set_fallback(app, handler, user, flags)` handles unmatched requests yourself (for example to
serve a single-page application).

### Reading the request

```c
const char* id = nyxj_req_param(req, "id");               /* route parameter, NULL if absent */
const char* auth = nyxj_req_header(req, "Authorization"); /* case-insensitive, NULL if absent */
char limit[16];
if (nyxj_req_query(req, "limit", limit, sizeof limit)) { /* URL-decoded query parameter */ }
nyxj_value* body = nyxj_req_json(req);                    /* parsed on first call, NULL if invalid */
const nyxj_error* why = nyxj_req_json_error(req);         /* why the body is not valid JSON */
```

The request also exposes `req->method`, `req->path`, `req->query` (raw), `req->body` and
`req->body_len` (raw bytes, NUL-terminated), `req->remote_addr`, `req->route` (the matched pattern) and
`req->headers`.

`nyxj_req_json` parses the body with the limits of `cfg.parse`, only once however many times you call
it. The tree belongs to the request: **do not free it**.

### Writing the response

```c
nyxj_res_json(res, 200, value);                        /* serializes and frees value */
nyxj_res_json_str(res, 200, "{\"ok\":true}");          /* JSON you already have as text */
nyxj_res_text(res, 200, "text/plain", "hello");
nyxj_res_bytes(res, 200, "application/octet-stream", data, len);
nyxj_res_error(res, 404, "not_found", "no such todo");  /* standard error body */
nyxj_res_header(res, "Cache-Control", "no-store");      /* extra header */
nyxj_res_status(res, 204);                              /* status only, no body */
```

`nyxj_res_error` always produces the same shape, which makes errors easy to handle for clients:

```json
{"error":"not_found","message":"no such todo","status":404}
```

If a handler sets no status, the response is `200` when it has a body and `204` otherwise. Header values
are cleaned of carriage returns and line feeds, so user input copied into a header cannot inject extra
headers.

### Middleware

A middleware runs **before** routing, for every request (except CORS preflights), in the order you
registered it. It can inspect or annotate the request, and it can answer directly and stop the chain by
returning `false`:

```c
static bool require_token(nyxj_request* req, nyxj_response* res, void* user) {
    const char* expected = user;
    if (strncmp(req->path, "/todos", 6) != 0) return true;   /* only protect the API */
    const char* auth = nyxj_req_header(req, "Authorization");
    if (auth && strcmp(auth, expected) == 0) return true;     /* continue to the route */
    nyxj_res_error(res, 401, "unauthorized", "missing or invalid Authorization header");
    return false;                                             /* stop: the response is ready */
}

nyxj_app_use(app, require_token, "Bearer secret-token");
```

Because middleware runs before routing, `req->route` and route parameters are not set yet: decide on
`req->path`.

### Where handlers run: the event loop and async routes

NyxJ serves all connections from one **event loop** thread. A normal handler runs on that thread, so
while it runs, no other request makes progress. Keep normal handlers short and never block in them (no
`sleep`, no slow file or network calls).

For heavy work, flag the route as **async**: it then runs on the worker pool and the event loop stays
free.

```c
nyxj_app_route_ex(app, "POST", "/reports", build_report, NULL, NYXJ_ROUTE_ASYNC, NULL);
```

Async handlers run in parallel, so they must follow the thread-safety rules of Chapter 22. The
application's key-value store and metrics are safe to use from them. When the queue is full, clients get
`503` (backpressure).

### Timers

```c
static void every_minute(nyxj_app* app, void* user) { /* runs on the event loop */ }
nyxj_app_add_timer(app, 60 * 1000, every_minute, NULL);
```

### Automatic documentation (OpenAPI)

Every route appears in an **OpenAPI 3** document at `/openapi.json`, rendered as an interactive page
at `/docs` (Swagger UI, loaded by the browser from a CDN). Describe routes to make it useful:

```c
nyxj_route_doc doc = {0};
doc.summary = "Create a todo";
doc.tag = "todos";
doc.request_schema = "{\"type\":\"object\",\"required\":[\"title\"],"
                     "\"properties\":{\"title\":{\"type\":\"string\"}}}";
nyxj_app_route_ex(app, "POST", "/todos", create_todo, NULL, 0, &doc);
```

Route parameters such as `:id` become OpenAPI path parameters (`{id}`) automatically.
`nyxj_app_openapi(app)` returns the document as text if you want to save or publish it.

### HTTPS

Set a certificate and its private key, in PEM format:

```c
cfg.listen_url = "https://0.0.0.0:8443";
cfg.tls_cert_file = "cert.pem";
cfg.tls_key_file = "key.pem";
```

The built-in TLS stack speaks TLS 1.3 and expects an elliptic-curve (P-256) certificate. For local tests,
create a self-signed one with OpenSSL:

```bash
openssl ecparam -name prime256v1 -genkey -noout -out key.pem
openssl req -new -x509 -key key.pem -out cert.pem -days 365 -subj "/CN=localhost"
```

In production, use a certificate from a certificate authority (for example Let's Encrypt), or terminate
TLS in a reverse proxy in front of NyxJ.

### Running and stopping

```c
nyxj_error err;
if (!nyxj_app_listen(app, &err)) { /* port busy, bad address... */ }   /* optional */
printf("port %d\n", nyxj_app_port(app));   /* the real port, useful with port 0 */
int rc = nyxj_app_run(app);                /* blocks until nyxj_app_stop() */
nyxj_app_destroy(app);
```

`nyxj_app_run` calls `nyxj_app_listen` itself if you did not. `nyxj_app_stop` can be called from
another thread or from a signal handler; `run` then finishes in-flight async jobs and returns. If you
need your own loop, call `nyxj_app_poll(app, timeout_ms)` repeatedly instead of `run`.

The application also gives you its services: `nyxj_app_kv(app)` (a key-value store), `nyxj_app_metrics(app)`,
`nyxj_app_pool(app)`, `nyxj_app_uptime(app)` and `nyxj_app_eco_level(app)`. Handlers reach the
application through `req->app`.

### Complete example: a todo API

```c
#include "nyxj_http.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>

static nyxj_app* app;

static void on_signal(int sig) {
    (void)sig;
    nyxj_app_stop(app);
}

static bool require_token(nyxj_request* req, nyxj_response* res, void* user) {
    if (strncmp(req->path, "/todos", 6) != 0) return true;
    const char* auth = nyxj_req_header(req, "Authorization");
    if (auth && strcmp(auth, (const char*)user) == 0) return true;
    nyxj_res_error(res, 401, "unauthorized", "missing or invalid Authorization header");
    return false;
}

/* GET /todos */
static void list_todos(nyxj_request* req, nyxj_response* res) {
    nyxj_kv* kv = nyxj_app_kv(req->app);
    nyxj_value* keys = nyxj_kv_keys(kv, "todo:");
    nyxj_value* list = nyxj_new_array();
    for (int i = 0; i < nyxj_size(keys); i++) {
        nyxj_value* todo = nyxj_kv_get(kv, nyxj_array_get(keys, i)->as.string);
        if (todo) nyxj_array_push(list, todo);
    }
    nyxj_free_value(keys);
    nyxj_res_json(res, 200, list);
}

/* POST /todos  {"title": "..."} */
static void create_todo(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = nyxj_req_json(req);
    if (!body) {
        nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message);
        return;
    }
    const char* title = nyxj_get_string(body, "title");
    if (!title || !title[0]) {
        nyxj_res_error(res, 422, "invalid_todo", "'title' must be a non-empty string");
        return;
    }
    nyxj_kv* kv = nyxj_app_kv(req->app);
    double id;
    nyxj_kv_incr(kv, "seq:todo", 1, &id);
    nyxj_value* todo = nyxj_new_object();
    nyxj_object_set_number(todo, "id", id);
    nyxj_object_set_string(todo, "title", title);
    nyxj_object_set_bool(todo, "done", false);
    char key[32], location[48];
    snprintf(key, sizeof key, "todo:%d", (int)id);
    snprintf(location, sizeof location, "/todos/%d", (int)id);
    nyxj_kv_set(kv, key, todo, 0);
    nyxj_res_header(res, "Location", location);
    nyxj_res_json(res, 201, todo);
}

/* GET /todos/:id */
static void get_todo(nyxj_request* req, nyxj_response* res) {
    char key[64];
    snprintf(key, sizeof key, "todo:%s", nyxj_req_param(req, "id"));
    nyxj_value* todo = nyxj_kv_get(nyxj_app_kv(req->app), key);
    if (!todo) {
        nyxj_res_error(res, 404, "not_found", "no such todo");
        return;
    }
    nyxj_res_json(res, 200, todo);
}

/* DELETE /todos/:id */
static void delete_todo(nyxj_request* req, nyxj_response* res) {
    char key[64];
    snprintf(key, sizeof key, "todo:%s", nyxj_req_param(req, "id"));
    if (!nyxj_kv_del(nyxj_app_kv(req->app), key)) {
        nyxj_res_error(res, 404, "not_found", "no such todo");
        return;
    }
    nyxj_res_status(res, 204);
}

int main(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://0.0.0.0:8080";
    cfg.title = "Todo API";
    cfg.max_body_size = 64 * 1024;
    cfg.parse.max_depth = 16;
    app = nyxj_app_create(&cfg);
    if (!app) return 1;

    nyxj_app_use(app, require_token, "Bearer secret-token");

    nyxj_route_doc doc = {0};
    doc.tag = "todos";
    doc.summary = "List todos";
    nyxj_app_route_ex(app, "GET", "/todos", list_todos, NULL, 0, &doc);
    doc.summary = "Create a todo";
    doc.request_schema = "{\"type\":\"object\",\"required\":[\"title\"],"
                         "\"properties\":{\"title\":{\"type\":\"string\"}}}";
    nyxj_app_route_ex(app, "POST", "/todos", create_todo, NULL, 0, &doc);
    nyxj_app_get(app, "/todos/:id", get_todo, NULL);
    nyxj_app_delete(app, "/todos/:id", delete_todo, NULL);

    signal(SIGINT, on_signal);    /* Ctrl+C */
    signal(SIGTERM, on_signal);   /* docker stop, systemctl stop, Kubernetes */
    int rc = nyxj_app_run(app);
    nyxj_app_destroy(app);
    return rc;
}
```

Try it with `curl`:

```bash
TOKEN='Authorization: Bearer secret-token'
curl -s -X POST localhost:8080/todos -H "$TOKEN" -d '{"title": "Read the NyxJ book"}'
curl -s -X POST localhost:8080/todos -H "$TOKEN" -d '{"title": "Write a service"}'
curl -s localhost:8080/todos -H "$TOKEN"
curl -s localhost:8080/todos/1 -H "$TOKEN"
curl -s -X POST localhost:8080/todos -H "$TOKEN" -d '{"title": '
curl -s -o /dev/null -w '%{http_code}\n' -X DELETE localhost:8080/todos/1 -H "$TOKEN"
curl -s localhost:8080/todos
```

```text
{"id":1,"title":"Read the NyxJ book","done":false}
{"id":2,"title":"Write a service","done":false}
[{"id":1,"title":"Read the NyxJ book","done":false},{"id":2,"title":"Write a service","done":false}]
{"id":1,"title":"Read the NyxJ book","done":false}
{"error":"invalid_json","message":"unexpected end of input at line 1, column 11 (expected a value)","status":400}
204
{"error":"unauthorized","message":"missing or invalid Authorization header","status":401}
```

Then open `http://localhost:8080/docs` to see the generated documentation. The `examples/api_example`
folder contains another complete service, with a small web page that calls it.

### Recap

- `nyxj_app_create`, register routes, `nyxj_app_run`: a service in a few lines.
- Patterns support `:params` and `*`; handlers read with `nyxj_req_*` and answer with `nyxj_res_*`.
- Middleware runs before routing and can stop the chain.
- Normal handlers run on the event loop and must be quick; heavy ones use `NYXJ_ROUTE_ASYNC`.
- OpenAPI documentation, HTTPS, health, readiness and metrics are built in.

---

## Chapter 27. Calling Other Services: the HTTP Client

Services talk to other services. NyxJ includes a synchronous HTTP and HTTPS client.

### The functions

```c
nyxj_http_response resp;
nyxj_error err;
bool ok = nyxj_http_get(url, timeout_ms, &resp, &err);
bool ok2 = nyxj_http_post_json(url, json_value, timeout_ms, &resp, &err);
bool ok3 = nyxj_http_request(&options, &resp, &err);       /* full control */

nyxj_value* json = nyxj_http_response_json(&resp, &err);   /* parse the body (you free it) */
char value[128];
nyxj_http_response_header(&resp, "Content-Type", value, sizeof value);
nyxj_http_response_free(&resp);                            /* always, after a successful call */
```

The functions return `false` only when no HTTP response was received (connection refused, timeout, TLS
failure...). A `404` or `500` is still a response: check `resp.status`. The response holds `status`,
`headers` (raw text), `body` (NUL-terminated) and `body_len`.

`nyxj_http_options` fields:

| Field | Default | Meaning |
|---|---|---|
| `url` | required | `http://` or `https://` |
| `method` | `GET` (or `POST` with a body) | Any HTTP method |
| `headers` | none | Extra headers, as `"Name: value\r\n"` lines |
| `body`, `body_len` | none | Request body |
| `content_type` | `application/json` with a body | |
| `timeout_ms` | 10 000 | Total time allowed |
| `ca_file` | `NULL` | PEM file of trusted certificate authorities: enables certificate verification |

### HTTPS and certificate verification

**Pitfall**: without `ca_file`, an `https://` request is **encrypted but not authenticated**: the client
does not check who it talks to, so a man-in-the-middle could impersonate the server. For anything that
matters, set `ca_file` to your system's certificate bundle (for example
`/etc/ssl/certs/ca-certificates.crt` on Debian and Ubuntu, `/etc/ssl/cert.pem` on macOS). The client then
verifies the certificate chain and the host name.

### Do not block the event loop

The client is synchronous: the calling thread waits for the answer. Never call it from a normal handler
(Chapter 26), which would freeze the whole server meanwhile. Call it from an `NYXJ_ROUTE_ASYNC` handler,
a worker task, or a program of your own.

### Example

Start `nyxj-server` (Chapter 31) on port 8000, then run:

```c
#include "nyxj.h"
#include "nyxj_http.h"
#include <stdio.h>

int main(void) {
    nyxj_error err;
    nyxj_http_response resp;

    if (!nyxj_http_get("http://127.0.0.1:8000/api/info", 5000, &resp, &err)) {
        printf("request failed: %s\n", err.message);
        return 1;
    }
    nyxj_value* info = nyxj_http_response_json(&resp, &err);
    printf("GET /api/info -> %d, %s %s\n", resp.status, nyxj_get_string(info, "engine"),
           nyxj_get_string(info, "version"));
    char server[64];
    if (nyxj_http_response_header(&resp, "Server", server, sizeof server)) printf("Server: %s\n", server);
    nyxj_free_value(info);
    nyxj_http_response_free(&resp);

    nyxj_result body = nyxj_parse("{\"query\": \"$..price\", \"data\": {\"items\": [{\"price\": 3}, {\"price\": 7}]}}");
    if (nyxj_http_post_json("http://127.0.0.1:8000/api/query", body.root, 5000, &resp, &err)) {
        printf("POST /api/query -> %d %s\n", resp.status, resp.body);
        nyxj_http_response_free(&resp);
    }
    nyxj_free_value(body.root);

    nyxj_http_options o = {0};
    o.method = "PUT";
    o.url = "http://127.0.0.1:8000/api/kv/greeting?ttl=60000";
    o.headers = "X-Request-Id: demo-1\r\n";
    o.body = "\"hello\"";
    o.body_len = 7;
    if (nyxj_http_request(&o, &resp, &err)) {
        printf("PUT /api/kv/greeting -> %d %s\n", resp.status, resp.body);
        nyxj_http_response_free(&resp);
    }
    return 0;
}
```

```text
GET /api/info -> 200, NyxJ 2.0.0
Server: NyxJ/2.0.0
POST /api/query -> 200 {"count":2,"matches":[3,7]}
PUT /api/kv/greeting -> 201 {"key":"greeting","stored":true}
```

### Recap

- `nyxj_http_get`, `nyxj_http_post_json` and `nyxj_http_request` cover every need.
- `false` means no response; otherwise check `resp.status`, then free the response.
- Set `ca_file` to verify HTTPS servers, and never call the client from a non-async handler.

---

## Chapter 28. Running in Production

A service is not finished when it answers requests: it must tell the platform it is healthy, expose its
metrics, behave under load and stop cleanly. NyxJ does most of this for you.

### Liveness: /health

`GET /health` answers `200` as long as the process is able to respond:

```json
{"status":"ok","service":"NyxJ Server","version":"2.0.0","engine":"2.0.0","uptime_s":1.35}
```

### Readiness: /ready

`GET /ready` says whether the service can accept **more** work. It answers `200` normally, and `503`
with the header `X-NyxJ-Scale-Hint: scale-out` when the worker queue is fuller than `ready_queue_ratio`
(90% by default) or when Eco-Throttle is in its critical zone:

```json
{"status":"ready","queue_ratio":0,"queued":0,"active_workers":0,"eco_level":0}
```

Load balancers stop sending traffic to a service that is not ready, and an autoscaler can use the hint to
add instances. With Kubernetes:

```yaml
livenessProbe:
  httpGet: { path: /health, port: 8000 }
  periodSeconds: 10
readinessProbe:
  httpGet: { path: /ready, port: 8000 }
  periodSeconds: 5
```

### Metrics: /metrics

`GET /metrics` (and `/api/metrics`) serve Prometheus metrics. Built in:

| Metric | Meaning |
|---|---|
| `nyxj_http_requests_total{method,route,status}` | Requests served, by route pattern and status |
| `nyxj_http_request_duration_seconds` | Request latency histogram |
| `nyxj_http_requests_in_flight` | Requests being processed |
| `nyxj_http_rejected_total` | Requests rejected (body too large, queue full...) |
| `nyxj_json_parse_duration_seconds`, `nyxj_json_parse_errors_total`, `nyxj_json_parsed_bytes_total` | Request body parsing |
| `nyxj_pool_*` | Worker pool: threads, queue depth, active workers, concurrency limit, completed tasks |
| `nyxj_kv_*` | Key-value store: keys, hits, misses |
| `nyxj_process_cpu_seconds_total`, `nyxj_process_resident_memory_bytes` | Process resources |
| `nyxj_system_load1`, `nyxj_cpu_temperature_celsius` | System load and CPU temperature, when the platform provides them |
| `nyxj_eco_throttle_level`, `nyxj_uptime_seconds`, `nyxj_build_info` | State and version |

Route labels use the **pattern** (`/todos/:id`), not the real path, so the number of series stays small.
Add your own metrics with `nyxj_app_metrics(app)` (Chapter 24). A Prometheus scrape configuration:

```yaml
scrape_configs:
  - job_name: nyxj
    static_configs:
      - targets: ["10.0.0.5:8000"]
```

### Logs

With `access_log` enabled, every request produces one `INFO` line with the method, path, status,
duration and response size:

```text
2026-11-02T14:43:21.927Z INFO  [http] POST /todos 201 0.070ms 50B
```

Switch to JSON lines with `nyxj_log_set_json(true)` when a log collector (Loki, Elasticsearch,
CloudWatch...) reads them.

### Heartbeat

Every `heartbeat_ms` (5 s by default) the application samples CPU time, memory, load and temperature,
removes expired keys from its key-value store and updates Eco-Throttle. If `heartbeat_url` is set, it
also POSTs a small JSON report there (from a worker thread, without blocking requests):

```json
{"service":"Todo API","version":"2.0.0","engine":"2.0.0","uptime_s":120.5,"rss_bytes":5242880,"load1":0.42,"eco_level":0}
```

### Eco-Throttle

On a fanless edge device or a shared machine, running every core flat out can overheat the hardware or
starve neighbors. Eco-Throttle lowers the number of active workers when the machine is under stress:

| Level | Condition (`eco_temp_c` = T, `eco_load` = L per CPU) | Effect |
|---|---|---|
| 0, normal | below the thresholds | All workers |
| 1, throttled | temperature at least T, or load at least L | Half the workers |
| 2, critical | temperature at least T + 10 °C, or load at least 1.5 x L | One worker; `/ready` answers `503` |

The level only goes down after three calm samples in a row, to avoid oscillating. Watch it with the
`nyxj_eco_throttle_level` metric or `nyxj_app_eco_level(app)`.

```c
cfg.eco_temp_c = 75;   /* throttle above 75 °C */
cfg.eco_load = 1.0;    /* or above a load of 1.0 per CPU */
```

### Graceful shutdown

Container platforms stop a service by sending `SIGTERM`, then kill it a few seconds later. Install a
signal handler that calls `nyxj_app_stop` (as in the todo example): `nyxj_app_run` then finishes the
async jobs in progress and returns, and you can save state (for example `nyxj_kv_save`) before exiting.

### Production checklist

- [ ] Limits set: `max_body_size`, `cfg.parse` depth, string and element limits.
- [ ] Inputs validated (schema or explicit checks); errors returned with `nyxj_res_error`.
- [ ] Heavy or blocking handlers flagged `NYXJ_ROUTE_ASYNC`.
- [ ] HTTPS enabled, or TLS terminated by a trusted proxy.
- [ ] `/health` and `/ready` wired to the orchestrator; `/metrics` scraped.
- [ ] `SIGTERM` handled; state saved on exit if needed.
- [ ] Logs in JSON for the collector, with no secrets in them.
- [ ] Tests run with `make SAN=1 test` in CI.

### Recap

- `/health` for liveness, `/ready` for readiness with a scale-out hint.
- `/metrics` exposes HTTP, parsing, pool, store and resource metrics; add yours.
- Heartbeat samples resources; Eco-Throttle protects hot or busy machines.
- Handle `SIGTERM` for a graceful shutdown.

---

## Chapter 29. Extending NyxJ with Plugins

### What a plugin is

A plugin is a shared library (`.so` on Linux and macOS, `.dll` on Windows) that a running NyxJ
application loads at startup to add routes and middleware. You can extend `nyxj-server`, or your own
application, without recompiling it.

### How it works

A plugin exports up to three functions:

```c
NYXJ_PLUGIN_EXPORT const nyxj_plugin_meta* nyxj_plugin_describe(void);           /* required */
NYXJ_PLUGIN_EXPORT int nyxj_plugin_init(nyxj_app* app, const nyxj_host_api* api); /* required, 0 = success */
NYXJ_PLUGIN_EXPORT void nyxj_plugin_shutdown(void);                              /* optional */
```

The host calls `describe` to read the plugin's name, version and API version, then `init`, passing a
**function table** (`nyxj_host_api`). The plugin calls NyxJ **only through this table**: `api->route`,
`api->req_json`, `api->new_object`, `api->res_json`, `api->log`, and so on.

**Why a table?** The plugin does not link against NyxJ at all. It does not matter whether the host was
built statically or dynamically, or which exact build it is: as long as the API version matches, the
plugin works. The table contains routing (`route`, `use`), request and response helpers, the JSON
essentials (parse, serialize, builders, `get_path`), logging, and the shared key-value store and metrics.
See `nyxj_plugin.h` for the complete list.

**Pitfall**: inside a plugin, never call `nyxj_*` functions directly (for example `nyxj_new_object()` or
the `NYXJ_INFO` macro): they are not linked into the plugin and loading would fail. Always go through
`api->...`. Reading request fields directly (`req->body`, `req->path`) is fine.

### A complete plugin

```c
#include "nyxj_plugin.h"
#include <stdio.h>

static const nyxj_host_api* api = NULL;

static void hello(nyxj_request* req, nyxj_response* res) {
    char text[160];
    snprintf(text, sizeof text, "Hello %s, from a plugin!", api->req_param(req, "name"));
    nyxj_value* o = api->new_object();
    api->object_set(o, "message", api->new_string(text));
    api->res_json(res, 200, o);
}

NYXJ_PLUGIN_EXPORT const nyxj_plugin_meta* nyxj_plugin_describe(void) {
    static const nyxj_plugin_meta meta = {"hello", "1.0.0", "Greeting routes", NYXJ_PLUGIN_API_VERSION};
    return &meta;
}

NYXJ_PLUGIN_EXPORT int nyxj_plugin_init(nyxj_app* app, const nyxj_host_api* host) {
    api = host;
    nyxj_route_doc doc = {0};
    doc.summary = "Greeting served by a plugin";
    doc.tag = "plugins";
    if (!api->route(app, "GET", "/hello/:name", hello, NULL, 0, &doc)) return 1;
    api->log(NYXJ_LOG_INFO, "hello-plugin", __FILE__, __LINE__, "routes registered");
    return 0;
}

NYXJ_PLUGIN_EXPORT void nyxj_plugin_shutdown(void) { api = NULL; }
```

The repository's `examples/plugin_hello/hello_plugin.c` is a slightly larger version of this plugin.

### Build and load

```bash
# Linux and macOS
gcc -std=c11 -O2 -fPIC -shared -I$NYXJ/include hello_plugin.c -o hello_plugin.so
# Windows
gcc -std=c11 -O2 -shared -I$NYXJ/include hello_plugin.c -o hello_plugin.dll

nyxj-server --plugin ./hello_plugin.so
curl http://localhost:8000/hello/Ada
```

In your own application: `nyxj_app_load_plugin(app, "./hello_plugin.so", &err)`, before `nyxj_app_run`.
`make plugin` builds the example plugin into `build/plugins/`.

### Rules

- `meta.api_version` must be `NYXJ_PLUGIN_API_VERSION`; the host refuses other versions.
- `init` returns `0` on success; any other value makes loading fail with `NYXJ_ERR_PLUGIN`.
- A plugin runs inside the host process with all its permissions: load only plugins you trust.

### Recap

- Plugins are shared libraries that add routes and middleware at startup.
- They export `describe` and `init` (and optionally `shutdown`) and use only the `nyxj_host_api` table.
- Load them with `--plugin` or `nyxj_app_load_plugin`.

---

## Chapter 30. Clustering (Experimental)

### The idea: gossip

When you run several NyxJ instances, they can discover each other and share information without any
central server, using a **gossip protocol**. Every second, each node sends what it knows about the
cluster (members, their addresses, their load) to a few other nodes over UDP. Like a rumor, information
reaches every node in a few rounds, and the cluster keeps working when any node disappears.

Each node also watches the others: a node that has not been heard from for 5 seconds becomes `suspect`,
and after 15 seconds `dead`. Each node announces its load, so any node can find the **least loaded** peer
and offload work to it.

### The functions

```c
nyxj_cluster_config cc = {0};
cc.bind_url = "udp://0.0.0.0:7946";            /* where this node listens for gossip */
cc.advertise_addr = "udp://10.0.0.5:7946";     /* the address peers should use to reach it */
cc.http_addr = "http://10.0.0.5:8000";         /* its HTTP address, announced to peers */
cc.node_name = "alpha";                         /* optional, random id by default */
const char* seeds[] = {"udp://10.0.0.6:7946"};  /* known members to join */
cc.seeds = seeds;
cc.seed_count = 1;

nyxj_error err;
nyxj_cluster* cluster = nyxj_cluster_start(app, &cc, &err);   /* runs on the app's event loop */

nyxj_value* members = nyxj_cluster_members(cluster);   /* array of member objects (you free it) */
int alive = nyxj_cluster_alive_count(cluster);
char peer[128];
if (nyxj_cluster_least_loaded(cluster, peer, sizeof peer)) { /* HTTP address of the best peer */ }
nyxj_cluster_stop(cluster);
```

The intervals are configurable with `gossip_interval_ms` (1000), `suspect_after_ms` (5000) and
`dead_after_ms` (15000). Each member object looks like:

```json
{"id":"9f2c41d07ab3","name":"beta","gossip":"udp://127.0.0.1:7947","http":"http://0.0.0.0:8002","hb":42,"load":0.1,"status":"alive","last_seen_ms_ago":310,"self":false}
```

### Try it with nyxj-server

```bash
nyxj-server --port 8001 --cluster-bind udp://0.0.0.0:7946 --cluster-name alpha
nyxj-server --port 8002 --cluster-bind udp://0.0.0.0:7947 --cluster-name beta --seed udp://127.0.0.1:7946
curl http://localhost:8001/cluster/members
```

### Limits

Clustering is **experimental**. Gossip messages are neither encrypted nor authenticated: use it only on a
trusted private network. Leader election and data replication are on the roadmap, not in 2.0.0.

### Recap

- Nodes discover each other and detect failures by gossiping over UDP, without a central server.
- Members go `alive`, `suspect`, then `dead`; `nyxj_cluster_least_loaded` finds a peer for offloading.
- Experimental and unauthenticated: trusted networks only.

---

## Chapter 31. nyxj-server: the Ready-Made Server

`nyxj-server` exposes the whole engine as an HTTP service. You can use NyxJ from any language, or
deploy it as a JSON firewall, without writing a line of C.

### Starting it

```bash
nyxj-server                       # http://0.0.0.0:8000
nyxj-server --port 9000
nyxj-server --config nyxj.json --threads 8
```

```text
NyxJ Micro-service v2.0.0 (SIMD: sse2)
Server active at: http://0.0.0.0:8000 (docs: /docs, metrics: /metrics)
Press Ctrl+C to stop.
```

Open `http://localhost:8000/docs` to explore and try every endpoint from your browser.

### Options

| Option | Config key | Description |
|---|---|---|
| `--listen URL` / `--port N` | `listen` | Listen address, default `http://0.0.0.0:8000` |
| `--threads N` | `threads` | Worker threads (0 = one per CPU, -1 = none) |
| `--queue N` | `queue` | Pending async jobs before answering 503 |
| `--max-body BYTES` | `max_body` | Largest accepted body (413 above), default 16 MiB |
| `--max-depth N` | `max_depth` | JSON nesting limit, default 512 |
| `--tls-cert FILE`, `--tls-key FILE` | `tls_cert`, `tls_key` | Enable HTTPS (PEM files) |
| `--log-level LEVEL` | `log_level` | `trace`, `debug`, `info`, `warn`, `error`, `off` |
| `--log-json` | `log_json` | Structured JSON logs |
| `--no-access-log` | `access_log` | Disable per-request log lines |
| `--no-cors` | `cors` | Disable CORS headers |
| `--plugin FILE` | `plugins` (array) | Load plugins (repeatable) |
| `--gate-upstream URL` | `gate_upstream` | Safe-Gate mode (see below) |
| `--kv-snapshot FILE` | `kv_snapshot` | Load the key-value store at start, save it on exit |
| `--heartbeat-ms N`, `--heartbeat-url URL` | `heartbeat_ms`, `heartbeat_url` | Resource sampling and webhook |
| `--eco-temp C`, `--eco-load L` | `eco_temp`, `eco_load` | Eco-Throttle thresholds |
| `--cluster-bind URL` | `cluster_bind` | Enable gossip clustering |
| `--cluster-advertise URL`, `--cluster-name NAME` | `cluster_advertise`, `cluster_name` | Node identity |
| `--seed URL` | `seeds` (array) | Gossip seeds (repeatable) |
| | `title` | Service name in `/health` and the documentation |
| `--config FILE` | | Load a configuration file |
| `--version`, `--help` | | Print the version or the help |

### The configuration file

The configuration file is JSON with comments and trailing commas allowed. Its keys mirror the options:

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

Options are applied from left to right: put `--config` first, and flags after it override the file.

### Endpoints

| Method | Path | Description |
|---|---|---|
| POST | `/api/data` | Parse and echo a document |
| POST | `/api/format?indent=N&sort=1` | Pretty-print |
| POST | `/api/validate` | `{"schema": ..., "data": ...}` → `{"valid", "error_count", "errors"}` (422 when invalid) |
| POST | `/api/query` | `{"query": "$..x", "data": ...}` → `{"count", "matches"}` |
| POST | `/api/repair` | Raw almost-JSON → `{"valid", "repaired", "report", "result"}` |
| POST | `/api/diff?mode=merge` | `{"from", "to"}` → JSON Patch (or Merge Patch) |
| POST | `/api/patch` | `{"document", "patch"}`: an array is a JSON Patch, an object a Merge Patch |
| POST | `/api/msgpack/encode` | JSON → `application/msgpack` |
| POST | `/api/msgpack/decode` | MessagePack → JSON |
| GET | `/api/kv?prefix=` | List keys |
| GET / PUT / DELETE | `/api/kv/:key` | Read, store (`?ttl=ms`) or delete a value |
| POST | `/api/kv/:key/incr?by=N` | Atomic increment |
| GET | `/api/info` | Version, SIMD backend, features |
| GET | `/health`, `/ready` | Liveness and readiness (Chapter 28) |
| GET | `/metrics`, `/api/metrics` | Prometheus metrics |
| GET | `/openapi.json`, `/docs` | OpenAPI document and Swagger UI |
| GET | `/cluster/members` | Cluster view (when clustering is enabled) |

Errors always have the shape `{"error": "<code>", "message": "...", "status": N}`.

### Examples with curl

```bash
curl -s -X POST localhost:8000/api/data -d '{"a":'
# {"error":"invalid_json","message":"unexpected end of input at line 1, column 6 (expected a value)","status":400}

curl -s -X POST localhost:8000/api/validate \
     -d '{"schema":{"type":"object","required":["name"]},"data":{"age":3}}'
# {"valid":false,"error_count":1,"errors":[{"path":"$","keyword":"required","message":"missing required property 'name'"}]}

curl -s -X POST localhost:8000/api/query -d '{"query":"$..price","data":{"items":[{"price":3},{"price":7}]}}'
# {"count":2,"matches":[3,7]}

curl -s -X POST localhost:8000/api/repair -d "{a: 1, b: 'x',}"
# {"valid":true,"repaired":"{\"a\":1,\"b\":\"x\"}","report":{"fixes":4,"kinds":["quotes","unquoted_key","trailing_comma"]},"result":{"a":1,"b":"x"}}

curl -s -X PUT "localhost:8000/api/kv/session:1?ttl=60000" -d '{"user":"ada"}'
# {"key":"session:1","stored":true}
curl -s localhost:8000/api/kv/session:1
# {"key":"session:1","value":{"user":"ada"},"ttl_ms":59894}
curl -s -X POST "localhost:8000/api/kv/hits/incr?by=5"
# {"key":"hits","value":5}
```

### Safe-Gate: a JSON firewall

Safe-Gate puts NyxJ in front of another service (written in Node.js, Python, PHP...) as a **reverse
proxy** that only lets clean JSON through:

```text
 client ──► nyxj-server :8080 ──(Guardian checks)──► your back-end :3000
                    │
                    └── bad payload: rejected with 400, 413 or 422, never reaches the back-end
```

```bash
nyxj-server --port 8080 --gate-upstream http://127.0.0.1:3000
```

Requests that match no NyxJ route are checked by the Guardian (Chapter 20) with its default policy: size,
UTF-8, depth, string and element limits. Bad payloads are rejected (`413` for size, `400` for malformed
JSON, `422` for the other violations) and counted in `nyxj_gate_blocked_total{reason}`. Clean ones are
forwarded with `X-Forwarded-For` and `X-NyxJ-Gate: passed` headers. Your back-end no longer has to worry
about JSON bombs. In your own application, the same mode is one call:
`nyxj_app_enable_gate_proxy(app, "http://127.0.0.1:3000", &policy)`.

### HTTPS, persistence and the rest

```bash
nyxj-server --port 8443 --tls-cert cert.pem --tls-key key.pem   # HTTPS (see Chapter 26 for the certificate)
nyxj-server --kv-snapshot store.json                             # keep the key-value store across restarts
nyxj-server --plugin build/plugins/hello_plugin.so               # Chapter 29
nyxj-server --cluster-bind udp://0.0.0.0:7946                    # Chapter 30
```

### Running it as a system service (Linux)

```ini
# /etc/systemd/system/nyxj.service
[Unit]
Description=NyxJ server
After=network.target

[Service]
ExecStart=/usr/local/bin/nyxj-server --config /etc/nyxj/nyxj.json --log-json
Restart=on-failure
User=nyxj

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now nyxj
```

`nyxj-server` handles `SIGTERM`: `systemctl stop` shuts it down cleanly and saves the key-value snapshot.

### Recap

- `nyxj-server` exposes parsing, validation, queries, repair, patches, MessagePack and a key-value
  store over HTTP.
- Configure it with flags or a JSON file; explore it at `/docs`.
- Safe-Gate mode turns it into a JSON firewall in front of any back-end.

---

# Part VI. Going Further

## Chapter 32. NyxJ in the Browser: WebAssembly

### What it is

**WebAssembly** (WASM) is a portable binary format that browsers, Node.js and edge platforms (Cloudflare
Workers, Deno, Fastly...) run at near-native speed. Because the NyxJ core is pure C with no threads and
no networking, it compiles to WebAssembly unchanged: the same parser, repair, query, schema and diff code
runs in your web page.

### Building it

Install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (the `emsdk` tool),
activate it in your shell, then:

```bash
make wasm
```

This produces `build/wasm/nyxj.js` (a loader) and `build/wasm/nyxj.wasm` (the engine). Published releases
also contain them in `nyxj-<version>-wasm.tar.gz`.

### The functions

Every function takes strings and returns a JSON **envelope** as a string allocated in WebAssembly memory:

```json
{"ok": true, "result": ...}
{"ok": false, "error": {"code": "UNEXPECTED_EOF", "message": "...", "line": 1, "column": 6}}
```

| Function | Arguments | `result` on success |
|---|---|---|
| `nyxj_wasm_version()` | | Returns the version string directly (no envelope) |
| `nyxj_wasm_format(json, indent)` | text, spaces (`0` = compact) | The formatted text |
| `nyxj_wasm_minify(json)` | text | The compact text |
| `nyxj_wasm_validate(json)` | text | `{"type": ..., "size": ...}` |
| `nyxj_wasm_query(json, path)` | text, JSONPath | Array of matches |
| `nyxj_wasm_repair(text)` | almost-JSON | `{"json": "...", "report": {...}}` |
| `nyxj_wasm_schema(schema, data)` | two texts | `{"valid": ..., "errors": [...]}` |
| `nyxj_wasm_diff(from, to)` | two texts | JSON Patch |
| `nyxj_wasm_msgpack_size(json)` | text | `{"json_bytes": ..., "msgpack_bytes": ...}` |
| `nyxj_wasm_free(ptr)` | a returned pointer | Releases the result string |

### Using it from Node.js

```js
// demo.cjs: node demo.cjs (next to nyxj.js and nyxj.wasm)
const NyxJ = require('./nyxj.js');

NyxJ().then((m) => {
  // Calls a function, converts the result to a JS object and frees the WASM memory.
  const call = (name, ...args) => {
    const types = args.map((a) => (typeof a === 'number' ? 'number' : 'string'));
    const ptr = m.ccall(name, 'number', types, args);
    const text = m.UTF8ToString(ptr);
    m._nyxj_wasm_free(ptr);
    return JSON.parse(text);
  };

  console.log(call('nyxj_wasm_repair', "{name: 'Ada', tags: ['c',],}").result.json);
  console.log(call('nyxj_wasm_query', '{"a":[{"p":1},{"p":5}]}', '$.a[?(@.p > 2)].p').result);
  console.log(call('nyxj_wasm_validate', '{"a":').error.message);
});
```

```text
{"name":"Ada","tags":["c"]}
[ 5 ]
unexpected end of input at line 1, column 6 (expected a value)
```

**Pitfall**: always free the returned pointer with `_nyxj_wasm_free`, as the helper does, or WebAssembly
memory grows with every call.

### Using it in a web page

`examples/wasm/index.html` is a small playground (repair, format, validate, query). Browsers refuse to
load WebAssembly from `file://` URLs, so serve the repository with any local web server:

```bash
python3 -m http.server 8080
# then open http://localhost:8080/examples/wasm/
```

### Recap

- `make wasm` compiles the core to `nyxj.js` + `nyxj.wasm` with Emscripten.
- Functions take strings and return a JSON envelope `{"ok", "result" | "error"}`.
- Free every result with `_nyxj_wasm_free`; serve pages over HTTP, not `file://`.

---

## Chapter 33. Building, Testing and Releasing

### Make targets

| Command | Result |
|---|---|
| `make` | Library, CLI, server, example plugin and examples in `build/` |
| `make lib`, `make cli`, `make server`, `make plugin`, `make examples` | One part only |
| `make test` | Builds and runs the test suite |
| `make SAN=1 test` | Tests under AddressSanitizer and UndefinedBehaviorSanitizer |
| `make wasm` | WebAssembly build (requires Emscripten) |
| `make dist PLATFORM=linux-x64` | Release layout in `dist/nyxj-<version>-<platform>/` |
| `make install PREFIX=/usr/local` | Installs binaries, library and headers (`DESTDIR` is supported) |
| `make version` | Prints the version read from `include/nyxj.h` |
| `make clean`, `make re` | Remove `build/` and `dist/`, rebuild everything |

Useful variables: `CC` (compiler), `AR`, `OPT` (default `-O2`), `CFLAGS` (extra flags), `PREFIX`,
`EMCC`. Cross-compiling, for example for 64-bit ARM Linux:

```bash
make CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-ar
```

### The test suite

```text
tests/
├── test.h            CHECK, CHECK_STR and RUN macros
├── test_main.c       Runs every suite and prints the summary
├── test_core.c       Parser, values, serializer, paths, arena, errors
├── test_modules.c    Mapping, schema, query, patch, msgpack, view, SAX, repair, gate, reliability
├── test_runtime.c    Thread pool, key-value store, metrics, IPC
├── test_http.c       Router, helpers, and a live server queried with the HTTP client
└── smoke.sh          End-to-end test of the nyxj and nyxj-server binaries
```

A test is a function using `CHECK(condition)` (and `CHECK_STR(a, b)` for strings), registered with
`RUN(test_name)` in its suite:

```c
static void test_new_feature(void) {
    nyxj_result r = nyxj_parse("[1, 2]");
    CHECK(r.is_valid);
    CHECK(nyxj_size(r.root) == 2);
    nyxj_free_value(r.root);
}
/* in suite_core(): */
RUN(test_new_feature);
```

Every behavior change comes with a test. When you touch memory handling, also run `make SAN=1 test` on
Linux or macOS: the sanitizers catch leaks, buffer overflows and undefined behavior that normal tests
miss.

### Continuous integration

Every push and pull request runs the `CI` workflow (`.github/workflows/ci.yml`) on Linux x64, Linux ARM64,
macOS, Windows (MSYS2), a sanitizer build, and the WebAssembly build with a Node.js smoke test. A pull
request is merged only when everything is green.

### Releasing (maintainers)

1. Set the new version in `include/nyxj.h` (`NYXJ_VERSION` and its parts).
2. Move the `CHANGELOG.md` entries under a `## [X.Y.Z] - YYYY-MM-DD` heading.
3. Merge to `main`, then tag and push: `git tag vX.Y.Z && git push origin vX.Y.Z`.

The `Release` workflow (`.github/workflows/release.yml`) checks that the tag matches `NYXJ_VERSION`,
builds and tests every platform, packages the archives and the WebAssembly build, computes SHA-256
checksums, writes release notes (changelog highlights, changes grouped by Conventional Commit type,
contributors) and publishes the GitHub release. Running the workflow manually performs a dry run that
uploads a preview instead of publishing.

### Recap

- `make`, `make test`, `make SAN=1 test`, `make wasm`, `make dist`, `make install`.
- Tests live in `tests/` and use `CHECK` and `RUN`; every change needs one.
- CI covers five platforms; pushing a `vX.Y.Z` tag publishes a release automatically.

---

## Chapter 34. Best Practices and Troubleshooting

### Choosing how to read JSON

| You need to... | Use | Chapter |
|---|---|---|
| Read and modify documents comfortably | Tree parsing (`nyxj_parse`) | 7 |
| Process many messages fast, read-only | Tree parsing into an arena | 10 |
| Fill your own structs | Mapping (`nyxj_map_parse`) | 12 |
| Read a few fields with no allocation | Zero-copy view | 17 |
| Process files or streams of any size | SAX streaming | 18 |
| Only check that input is safe and valid | Guardian (`nyxj_gate_check`) or `nyxj_sax_validate` | 18, 20 |

### Good habits

- **Check every result**: `is_valid`, `NULL` returns, `false` returns. Print `error.message`: it is
  written for humans.
- **Know who owns what** (Chapter 6): free roots and returned buffers, never borrowed pointers.
- **Set limits** on everything that comes from outside (Chapters 7 and 20).
- **Validate early** with a schema, then trust the data in the rest of the program.
- **Keep handlers fast**; mark heavy ones `NYXJ_ROUTE_ASYNC`.
- **Reuse** parsed schemas, compiled queries and arenas instead of recreating them for each message.
- **Develop with sanitizers** (`-fsanitize=address,undefined`) and test with `make SAN=1 test`.
- **Measure** with `nyxj bench` and the `/metrics` endpoint before optimizing.

### Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| `undefined reference to nyxj_...` | `libnyxj.a` is missing or placed before your source file on the command line; it must come after it |
| `undefined reference to pthread_...`, `WSAStartup`, `dlopen`... | Missing system libraries: add `-lpthread -lm -ldl -lrt` (Linux), `-lpthread -lm` (macOS) or `-lws2_32 -lpsapi` (Windows) |
| Crash right after parsing | `is_valid` was not checked and `r.root` is `NULL`, or a borrowed pointer was used after the tree was freed |
| Memory keeps growing | A tree or buffer is never freed: run with AddressSanitizer to find it |
| `invalid UTF-8 byte sequence` with text typed in a Windows terminal | The terminal sent text in a legacy code page, not UTF-8. Use files saved as UTF-8, or `\u` escapes |
| JSON Pointer argument becomes `C:/...` in Git Bash | MSYS path conversion: prefix the command with `MSYS_NO_PATHCONV=1` |
| `cannot listen on http://0.0.0.0:8000` | The port is used by another program: choose another with `--port` or `listen_url` |
| `413 Payload Too Large` | The body exceeds `max_body_size` (`--max-body`) |
| `503 Service Unavailable` | The worker queue is full: add workers, raise `queue_capacity`, or scale out |
| `/docs` shows a blank page | Swagger UI is loaded from a CDN: the browser needs internet access. `/openapi.json` always works |
| HTTPS server refuses connections | The built-in TLS stack expects an EC P-256 certificate (Chapter 26) |
| Plugin fails to load | It calls `nyxj_*` directly instead of `api->...`, targets another API version, or `init` returned non-zero |

### Frequently asked questions

**Is NyxJ thread-safe?** The parser, serializer and read-only operations are, and so are the pool, the
key-value store and metrics. Modifying one tree from several threads needs your own lock (Chapter 22).

**Does it keep the order of object members?** Yes, members stay in document order, and setting an
existing key keeps its position. Use `sort_keys` for canonical output.

**What about big integers?** Numbers are `double`: integers are exact up to 2^53. Transmit larger
identifiers as strings.

**Can I use it from C++?** Yes, every header has `extern "C"` guards.

**Does it build with Microsoft Visual C++ (MSVC)?** Not currently. On Windows, use MinGW-w64 (GCC or
Clang).

**Can I use NyxJ in a commercial product?** Yes, embedding is free, including in commercial products.
Read the license summary in Chapter 35 and the [LICENSE](LICENSE) file for the conditions.

---

## Chapter 35. Where to Go Next

Congratulations: you have toured the whole engine, from parsing a string to running a cluster of
services. Here is where to continue.

- **Practice**: rebuild the todo API of Chapter 26 with a schema (Chapter 13) and struct mapping (Chapter
  12), then put it behind Safe-Gate (Chapter 31).
- **Read the examples** in `examples/`: an API service with a web page, an HTTP client, struct mapping,
  streaming, a plugin and a browser playground.
- **Keep the quick references at hand**: [docs/API.md](docs/API.md) for the C API and
  [docs/SERVER.md](docs/SERVER.md) for `nyxj-server`. Each header in `include/` documents its functions.
- **Follow the project**: [CHANGELOG.md](CHANGELOG.md) lists what changed in each version and
  [ROADMAP.md](ROADMAP.md) what comes next (embedded scripting, WebSockets, schema compilation,
  persistent storage, consensus, new SIMD paths, embedded SDKs...).
- **Contribute**: bug reports, ideas and pull requests are welcome. Read
  [CONTRIBUTING.md](CONTRIBUTING.md) first: it explains the workflow, the coding standards and the
  Conventional Commits used by the release notes.

### License in short

NyxJ is released under the **Fordima Public License (FPL) v1.0** ([LICENSE](LICENSE)). It is free for
individuals, education and companies, including inside commercial products. Building a similar or
competing engine from the source, redistributing it as a standalone product, or offering a paid hosted
version requires a commercial license from Fordima Inc. The networking layer (`vendor/mongoose`) is
Mongoose by Cesanta, licensed under GPL-2.0-only or a commercial license; binaries that include it
(`nyxj-server`, the HTTP parts of `libnyxj.a`) are subject to those terms, while the core engine and the
WebAssembly build do not use it.

### Getting help

Open an issue on [GitHub](https://github.com/fomadev/nyxj/issues) with your NyxJ version (`nyxj
version`), your platform and a minimal example. For a specific integration or professional support,
contact FomaDev, a Fordima Inc. company, through the repository.

---

## Appendix A. Error Codes

`nyxj_error_name(code)` returns the name, `nyxj_strerror(code)` the description. In C, each name is
prefixed with `NYXJ_ERR_` (except `NYXJ_OK`).

| Name | Description |
|---|---|
| `OK` | success |
| `NULL_INPUT` | input is NULL |
| `OUT_OF_MEMORY` | out of memory |
| `UNEXPECTED_EOF` | unexpected end of input |
| `UNEXPECTED_CHAR` | unexpected character |
| `INVALID_LITERAL` | invalid literal (expected true, false or null) |
| `INVALID_NUMBER` | invalid number |
| `INVALID_STRING` | invalid or unterminated string |
| `INVALID_ESCAPE` | invalid escape sequence |
| `INVALID_UNICODE` | invalid \u escape or surrogate pair |
| `INVALID_UTF8` | invalid UTF-8 byte sequence |
| `CONTROL_CHAR` | unescaped control character in string |
| `TRAILING_DATA` | unexpected data after the JSON value |
| `DUPLICATE_KEY` | duplicate object key |
| `DEPTH_LIMIT` | maximum nesting depth exceeded |
| `SIZE_LIMIT` | input exceeds the maximum allowed size |
| `STRING_LIMIT` | string exceeds the maximum allowed length |
| `ELEMENT_LIMIT` | container exceeds the maximum number of elements |
| `TYPE_MISMATCH` | value has an unexpected type |
| `NOT_FOUND` | value not found |
| `MISSING_FIELD` | required field is missing |
| `SCHEMA_VIOLATION` | document does not match the schema |
| `INVALID_SCHEMA` | invalid schema |
| `INVALID_QUERY` | invalid query expression |
| `INVALID_POINTER` | invalid JSON pointer |
| `PATCH_FAILED` | patch operation failed |
| `TEST_FAILED` | patch test operation failed |
| `MSGPACK` | invalid MessagePack data |
| `INTEGRITY` | integrity check failed (shadow execution mismatch) |
| `IO` | input/output error |
| `NETWORK` | network error |
| `TIMEOUT` | operation timed out |
| `TLS` | TLS error |
| `QUEUE_FULL` | work queue is full |
| `PLUGIN` | plugin error |
| `UNSUPPORTED` | operation not supported on this platform |
| `INVALID_ARGUMENT` | invalid argument |

## Appendix B. API Index

| Header | Main functions |
|---|---|
| `nyxj.h` | `nyxj_parse`, `nyxj_parse_n`, `nyxj_parse_ex`, `nyxj_parse_file`, `nyxj_read_file`, `nyxj_free_value`, `nyxj_new_*`, `nyxj_array_*`, `nyxj_object_*`, `nyxj_clone`, `nyxj_equals`, `nyxj_size`, `nyxj_get_*`, `nyxj_path_*`, `nyxj_serialize*`, `nyxj_arena_*`, `nyxj_strerror`, `nyxj_error_name`, `nyxj_version`, `nyxj_simd_backend` |
| `nyxj_log.h` | `NYXJ_TRACE` ... `NYXJ_FATAL`, `nyxj_log_set_level`, `nyxj_log_set_json`, `nyxj_log_set_stream`, `nyxj_log_set_sink` |
| `nyxj_map.h` | `NYXJ_FIELD_*`, `nyxj_map_parse`, `nyxj_map_to_struct`, `nyxj_struct_to_json`, `nyxj_struct_to_value`, `nyxj_map_free`, `nyxj_generate_struct` |
| `nyxj_schema.h` | `nyxj_schema_validate`, `nyxj_schema_validate_json`, `nyxj_schema_report_to_value` |
| `nyxj_query.h` | `nyxj_query`, `nyxj_query_first`, `nyxj_query_count`, `nyxj_query_compile`, `nyxj_query_exec`, `nyxj_query_free` |
| `nyxj_patch.h` | `nyxj_pointer_get`, `nyxj_diff`, `nyxj_patch_apply`, `nyxj_merge_diff`, `nyxj_merge_patch` |
| `nyxj_msgpack.h` | `nyxj_msgpack_encode`, `nyxj_msgpack_decode`, `nyxj_json_to_msgpack`, `nyxj_msgpack_to_json` |
| `nyxj_view.h` | `nyxj_view_parse`, `nyxj_view_free`, `nyxj_view_path`, `nyxj_view_find`, `nyxj_view_index`, `nyxj_view_eq`, `nyxj_view_raw`, `nyxj_view_string`, `nyxj_view_number`, `nyxj_view_bool`, `nyxj_view_to_value` |
| `nyxj_sax.h` | `nyxj_sax_create`, `nyxj_sax_feed`, `nyxj_sax_finish`, `nyxj_sax_error`, `nyxj_sax_memory`, `nyxj_sax_documents`, `nyxj_sax_reset`, `nyxj_sax_destroy`, `nyxj_sax_validate` |
| `nyxj_repair.h` | `nyxj_repair`, `nyxj_parse_lenient`, `nyxj_repair_kind_name`, `nyxj_repair_report_to_value` |
| `nyxj_gate.h` | `nyxj_gate_policy_default`, `nyxj_gate_policy_strict`, `nyxj_gate_check` |
| `nyxj_reliability.h` | `nyxj_checksum`, `nyxj_parse_verified`, `nyxj_tmr_create`, `nyxj_tmr_read`, `nyxj_tmr_write`, `nyxj_tmr_free` |
| `nyxj_pool.h` | `nyxj_pool_create`, `nyxj_pool_submit`, `nyxj_pool_wait`, `nyxj_pool_set_concurrency`, `nyxj_pool_stats_get`, `nyxj_pool_destroy` |
| `nyxj_kv.h` | `nyxj_kv_create`, `nyxj_kv_set`, `nyxj_kv_get`, `nyxj_kv_del`, `nyxj_kv_incr`, `nyxj_kv_expire`, `nyxj_kv_ttl`, `nyxj_kv_keys`, `nyxj_kv_save`, `nyxj_kv_load`, `nyxj_kv_destroy` |
| `nyxj_metrics.h` | `nyxj_metrics_create`, `nyxj_metrics_counter`, `nyxj_metrics_gauge`, `nyxj_metrics_histogram`, `nyxj_metric_inc` / `add` / `set` / `observe`, `nyxj_metrics_render`, `nyxj_sys_stats_get` |
| `nyxj_ipc.h` | `nyxj_ipc_listen`, `nyxj_ipc_accept`, `nyxj_ipc_connect`, `nyxj_ipc_send_json`, `nyxj_ipc_recv_json`, `nyxj_ipc_close`, `nyxj_shm_create`, `nyxj_shm_open`, `nyxj_shm_write`, `nyxj_shm_read`, `nyxj_shm_close` |
| `nyxj_http.h` | `nyxj_app_create`, `nyxj_app_route(_ex)`, `nyxj_app_get` / `post` / `put` / `delete`, `nyxj_app_use`, `nyxj_app_run`, `nyxj_app_stop`, `nyxj_req_*`, `nyxj_res_*`, `nyxj_app_enable_gate_proxy`, `nyxj_http_get`, `nyxj_http_post_json`, `nyxj_http_request` |
| `nyxj_plugin.h` | `nyxj_plugin_describe`, `nyxj_plugin_init`, `nyxj_plugin_shutdown` (exported by plugins), `nyxj_host_api_get` |
| `nyxj_cluster.h` | `nyxj_cluster_start`, `nyxj_cluster_members`, `nyxj_cluster_alive_count`, `nyxj_cluster_least_loaded`, `nyxj_cluster_stop` |

## Appendix C. Glossary

| Term | Meaning |
|---|---|
| **Arena** | A memory pool that allocates in large blocks and releases everything at once |
| **Async route** | A route whose handler runs on the worker pool instead of the event loop |
| **Backpressure** | Refusing new work when a queue is full instead of letting it grow without bound |
| **Borrowed pointer** | A pointer into a structure owned by someone else; never freed by you |
| **DoS (denial of service)** | An attack that makes a service unavailable, for example by exhausting memory |
| **Event loop** | The single thread that handles all network connections of the server |
| **Gossip** | A protocol where nodes periodically share what they know with random peers |
| **JSON Pointer** | A string such as `/a/b/0` that designates one value (RFC 6901) |
| **JSONPath** | A query language that selects values in a document; NyxQuery implements it |
| **Middleware** | A function run before routing, able to inspect or reject a request |
| **NDJSON** | Newline-delimited JSON: one document per line |
| **OpenAPI** | A standard JSON description of an HTTP API, rendered by tools like Swagger UI |
| **Parsing** | Turning JSON text into an in-memory structure |
| **Prometheus** | A monitoring system that scrapes metrics from services over HTTP |
| **SAX** | Event-driven parsing: callbacks are called for each element, no tree is built |
| **Serialization** | Turning an in-memory structure into JSON text |
| **SIMD** | CPU instructions that process several bytes at once (SSE2, NEON) |
| **TLS** | The encryption protocol behind HTTPS |
| **TMR** | Triple modular redundancy: three copies and a majority vote |
| **TTL** | Time to live: how long a key-value entry remains before it expires |
| **UTF-8** | The standard encoding of text in JSON |
| **Zero-copy** | Reading data in place, without copying it into new memory |
