/*
 * nyxj: command-line toolbox for the NyxJ engine.
 * Run `nyxj help` for the list of commands.
 */
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "../../include/nyxj.h"
#include "../../include/nyxj_gate.h"
#include "../../include/nyxj_map.h"
#include "../../include/nyxj_msgpack.h"
#include "../../include/nyxj_patch.h"
#include "../../include/nyxj_query.h"
#include "../../include/nyxj_reliability.h"
#include "../../include/nyxj_repair.h"
#include "../../include/nyxj_sax.h"
#include "../../include/nyxj_schema.h"
#include "../../include/nyxj_view.h"
#include "../common/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int indent_opt = 2;
static const char* output_path = NULL;

static int fail(const char* what, const nyxj_error* err) {
    fprintf(stderr, "nyxj: %s: %s [%s]\n", what, err->message, nyxj_error_name(err->code));
    return 1;
}

static int write_output(const void* data, size_t len, bool newline) {
    FILE* f = output_path ? fopen(output_path, "wb") : stdout;
    if (!f) { perror(output_path); return 1; }
#ifdef _WIN32
    if (!output_path) _setmode(_fileno(stdout), _O_BINARY);
#endif
    fwrite(data, 1, len, f);
    if (newline) fputc('\n', f);
    if (f != stdout) fclose(f);
    return 0;
}

static int print_value(const nyxj_value* v, int indent) {
    nyxj_serialize_options o = {indent, false, false};
    size_t n;
    char* text = nyxj_serialize_ex(v, &o, &n);
    if (!text) return 1;
    int rc = write_output(text, n, true);
    free(text);
    return rc;
}

static nyxj_value* load(const char* path, nyxj_error* err) { return nyxj_parse_file(path, NULL, err); }

/* --- Commands --- */

static int cmd_validate(int argc, char** argv) {
    const char* path = argc > 0 ? argv[0] : "-";
    size_t len;
    nyxj_error err;
    char* data = nyxj_read_file(path, &len, &err);
    if (!data) return fail(path, &err);
    nyxj_result r = nyxj_parse_n(data, len);
    free(data);
    if (!r.is_valid) return fail(path, &r.error);
    printf("%s: valid JSON (%s, %d top-level %s)\n", path, nyxj_type_name(r.root->type), nyxj_size(r.root),
           r.root->type == NYXJ_OBJECT ? "members" : r.root->type == NYXJ_ARRAY ? "items" : "bytes");
    nyxj_free_value(r.root);
    return 0;
}

static int cmd_format(int argc, char** argv, int indent) {
    nyxj_error err;
    nyxj_value* v = load(argc > 0 ? argv[0] : "-", &err);
    if (!v) return fail("format", &err);
    int rc = print_value(v, indent);
    nyxj_free_value(v);
    return rc;
}

static int cmd_query(int argc, char** argv) {
    if (argc < 1) { fprintf(stderr, "usage: nyxj query '<jsonpath>' [file]\n"); return 2; }
    nyxj_error err;
    nyxj_value* v = load(argc > 1 ? argv[1] : "-", &err);
    if (!v) return fail("query", &err);
    nyxj_value* matches = nyxj_query(v, argv[0], &err);
    nyxj_free_value(v);
    if (!matches) return fail("query", &err);
    int rc = print_value(matches, indent_opt);
    nyxj_free_value(matches);
    return rc;
}

static int cmd_get(int argc, char** argv) {
    if (argc < 1) { fprintf(stderr, "usage: nyxj get <path|/pointer> [file]\n"); return 2; }
    nyxj_error err;
    nyxj_value* v = load(argc > 1 ? argv[1] : "-", &err);
    if (!v) return fail("get", &err);
    const nyxj_value* found = argv[0][0] == '/' || argv[0][0] == '\0' ? nyxj_pointer_get(v, argv[0]) : nyxj_get_path(v, argv[0]);
    int rc = 1;
    if (!found) fprintf(stderr, "nyxj: '%s' not found\n", argv[0]);
    else if (found->type == NYXJ_STRING) rc = write_output(found->as.string, strlen(found->as.string), true);
    else rc = print_value(found, indent_opt);
    nyxj_free_value(v);
    return rc;
}

static int cmd_repair(int argc, char** argv) {
    size_t len, n;
    nyxj_error err;
    char* data = nyxj_read_file(argc > 0 ? argv[0] : "-", &len, &err);
    if (!data) return fail("repair", &err);
    nyxj_repair_report rep;
    char* fixed = nyxj_repair(data, len, &n, &rep);
    free(data);
    if (!fixed) { fprintf(stderr, "nyxj: repair: nothing could be recovered\n"); return 1; }
    nyxj_result r = nyxj_parse_n(fixed, n);
    free(fixed);
    if (!r.is_valid) return fail("repair", &r.error);
    fprintf(stderr, "nyxj: %d fix(es):", rep.fixes);
    for (int i = 0; i < NYXJ_FIX_KIND_COUNT; i++)
        if (rep.kinds & (1u << i)) fprintf(stderr, " %s", nyxj_repair_kind_name(1u << i));
    fprintf(stderr, "\n");
    int rc = print_value(r.root, indent_opt);
    nyxj_free_value(r.root);
    return rc;
}

static int cmd_diff(int argc, char** argv, bool merge) {
    if (argc < 2) { fprintf(stderr, "usage: nyxj diff [--merge] <from.json> <to.json>\n"); return 2; }
    nyxj_error err;
    nyxj_value* a = load(argv[0], &err);
    if (!a) return fail(argv[0], &err);
    nyxj_value* b = load(argv[1], &err);
    if (!b) { nyxj_free_value(a); return fail(argv[1], &err); }
    nyxj_value* d = merge ? nyxj_merge_diff(a, b) : nyxj_diff(a, b);
    int rc = print_value(d, indent_opt);
    nyxj_free_value(a);
    nyxj_free_value(b);
    nyxj_free_value(d);
    return rc;
}

static int cmd_patch(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: nyxj patch <document.json> <patch.json>\n"); return 2; }
    nyxj_error err;
    nyxj_value* doc = load(argv[0], &err);
    if (!doc) return fail(argv[0], &err);
    nyxj_value* patch = load(argv[1], &err);
    if (!patch) { nyxj_free_value(doc); return fail(argv[1], &err); }
    int rc;
    if (patch->type == NYXJ_OBJECT) {
        doc = nyxj_merge_patch(doc, patch);
        rc = print_value(doc, indent_opt);
    } else if (nyxj_patch_apply(&doc, patch, &err)) {
        rc = print_value(doc, indent_opt);
    } else {
        rc = fail("patch", &err);
    }
    nyxj_free_value(doc);
    nyxj_free_value(patch);
    return rc;
}

static int cmd_schema(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: nyxj schema <schema.json> <document.json>\n"); return 2; }
    nyxj_error err;
    nyxj_value* s = load(argv[0], &err);
    if (!s) return fail(argv[0], &err);
    nyxj_value* d = load(argv[1], &err);
    if (!d) { nyxj_free_value(s); return fail(argv[1], &err); }
    nyxj_schema_report rep;
    bool ok = nyxj_schema_validate(s, d, &rep);
    if (ok) printf("%s: valid against %s\n", argv[1], argv[0]);
    for (int i = 0; i < rep.count; i++)
        printf("  %s: [%s] %s\n", rep.issues[i].path, rep.issues[i].keyword, rep.issues[i].message);
    if (rep.total > rep.count) printf("  ... and %d more\n", rep.total - rep.count);
    nyxj_free_value(s);
    nyxj_free_value(d);
    return ok ? 0 : 1;
}

static int cmd_msgpack(int argc, char** argv) {
    if (argc < 1 || (strcmp(argv[0], "encode") && strcmp(argv[0], "decode"))) {
        fprintf(stderr, "usage: nyxj msgpack encode|decode [file] [-o out]\n");
        return 2;
    }
    size_t len;
    nyxj_error err;
    char* data = nyxj_read_file(argc > 1 ? argv[1] : "-", &len, &err);
    if (!data) return fail("msgpack", &err);
    int rc;
    if (argv[0][0] == 'e') {
        size_t n;
        uint8_t* bin = nyxj_json_to_msgpack(data, len, &n, &err);
        rc = bin ? write_output(bin, n, false) : fail("msgpack", &err);
        if (bin) fprintf(stderr, "nyxj: %zu bytes of JSON -> %zu bytes of MessagePack (%.1f%%)\n", len, n, len ? 100.0 * n / len : 0);
        free(bin);
    } else {
        nyxj_value* v = nyxj_msgpack_decode((const uint8_t*)data, len, &err);
        rc = v ? print_value(v, indent_opt) : fail("msgpack", &err);
        nyxj_free_value(v);
    }
    free(data);
    return rc;
}

static int cmd_gen_struct(int argc, char** argv) {
    const char* name = "Document";
    const char* path = "-";
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--name") && i + 1 < argc) name = argv[++i];
        else path = argv[i];
    }
    nyxj_error err;
    nyxj_value* v = load(path, &err);
    if (!v) return fail("gen-struct", &err);
    char* code = nyxj_generate_struct(v, name);
    nyxj_free_value(v);
    if (!code) { fprintf(stderr, "nyxj: gen-struct: the sample must be an object (or an array of objects)\n"); return 1; }
    int rc = write_output(code, strlen(code), false);
    free(code);
    return rc;
}

typedef struct { uint64_t objects, arrays, strings, numbers, literals, keys; int depth, max_depth; } stream_stats;
static bool s_obj(void* u) { stream_stats* s = u; s->objects++; if (++s->depth > s->max_depth) s->max_depth = s->depth; return true; }
static bool s_arr(void* u) { stream_stats* s = u; s->arrays++; if (++s->depth > s->max_depth) s->max_depth = s->depth; return true; }
static bool s_end(void* u) { ((stream_stats*)u)->depth--; return true; }
static bool s_key(void* u, const char* k, size_t n) { (void)k; (void)n; ((stream_stats*)u)->keys++; return true; }
static bool s_str(void* u, const char* k, size_t n) { (void)k; (void)n; ((stream_stats*)u)->strings++; return true; }
static bool s_num(void* u, double v, const char* r, size_t n) { (void)v; (void)r; (void)n; ((stream_stats*)u)->numbers++; return true; }
static bool s_bool(void* u, bool b) { (void)b; ((stream_stats*)u)->literals++; return true; }
static bool s_null(void* u) { ((stream_stats*)u)->literals++; return true; }

static int cmd_stream(int argc, char** argv) {
    const char* path = argc > 0 ? argv[0] : "-";
    FILE* f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    stream_stats st;
    memset(&st, 0, sizeof(st));
    nyxj_sax_handler h = {s_obj, s_end, s_arr, s_end, s_key, s_str, s_num, s_bool, s_null, NULL};
    nyxj_sax_options o;
    memset(&o, 0, sizeof(o));
    o.multi_document = true;
    nyxj_sax* sax = nyxj_sax_create(&h, &st, &o);
    char chunk[4096];
    size_t n;
    bool ok = sax != NULL;
    while (ok && (n = fread(chunk, 1, sizeof(chunk), f)) > 0) ok = nyxj_sax_feed(sax, chunk, n);
    if (ok) ok = nyxj_sax_finish(sax);
    if (f != stdin) fclose(f);
    if (!ok) {
        int rc = sax ? fail("stream", nyxj_sax_error(sax)) : 1;
        nyxj_sax_destroy(sax);
        return rc;
    }
    printf("documents: %llu\nbytes: %llu\nobjects: %llu\narrays: %llu\nkeys: %llu\nstrings: %llu\nnumbers: %llu\n"
           "literals: %llu\nmax depth: %d\nparser memory: %zu bytes (constant)\n",
           (unsigned long long)nyxj_sax_documents(sax), (unsigned long long)nyxj_sax_bytes(sax),
           (unsigned long long)st.objects, (unsigned long long)st.arrays, (unsigned long long)st.keys,
           (unsigned long long)st.strings, (unsigned long long)st.numbers, (unsigned long long)st.literals,
           st.max_depth, nyxj_sax_memory(sax));
    nyxj_sax_destroy(sax);
    return 0;
}

static int cmd_gate(int argc, char** argv) {
    const char* path = "-";
    nyxj_gate_policy p = nyxj_gate_policy_default();
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--strict")) p = nyxj_gate_policy_strict();
        else path = argv[i];
    }
    size_t len;
    nyxj_error err;
    char* data = nyxj_read_file(path, &len, &err);
    if (!data) return fail(path, &err);
    nyxj_gate_verdict v;
    bool ok = nyxj_gate_check(data, len, &p, &v);
    free(data);
    if (ok) printf("%s: allowed\n", path);
    else printf("%s: blocked (%s): %s\n", path, v.reason, v.error.message);
    return ok ? 0 : 1;
}

static int cmd_tokens(int argc, char** argv) {
    size_t len;
    nyxj_error err;
    char* data = nyxj_read_file(argc > 0 ? argv[0] : "-", &len, &err);
    if (!data) return fail("tokens", &err);
    nyxj_view view;
    memset(&view, 0, sizeof(view));
    if (!nyxj_view_parse(&view, data, len, NULL, 0, &err)) { free(data); return fail("tokens", &err); }
    static const char* names[] = {"none", "object", "array", "string", "number", "true", "false", "null"};
    size_t shown = view.count < 50 ? view.count : 50;
    for (size_t i = 0; i < shown; i++) {
        size_t n;
        const char* raw = nyxj_view_raw(&view, i, &n);
        printf("#%-4zu %-7s size=%-4u [%u..%u) %.*s\n", i, names[view.tokens[i].type], view.tokens[i].size,
               view.tokens[i].start, view.tokens[i].end, (int)(n > 40 ? 40 : n), raw);
    }
    if (view.count > shown) printf("... %zu more tokens\n", view.count - shown);
    printf("%zu tokens, zero string copies\n", view.count);
    nyxj_view_free(&view);
    free(data);
    return 0;
}

static int cmd_bench(int argc, char** argv) {
    const char* path = NULL;
    int iterations = 50;
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--iterations") && i + 1 < argc) iterations = atoi(argv[++i]);
        else path = argv[i];
    }
    if (!path) { fprintf(stderr, "usage: nyxj bench <file> [--iterations N]\n"); return 2; }
    size_t len;
    nyxj_error err;
    char* data = nyxj_read_file(path, &len, &err);
    if (!data) return fail(path, &err);
    if (iterations < 1) iterations = 1;
    double mb = (double)len * iterations / (1024.0 * 1024.0);
    printf("NyxJ %s benchmark: %s (%zu bytes x %d), SIMD: %s\n", NYXJ_VERSION, path, len, iterations, nyxj_simd_backend());

    uint64_t t0 = nyxj_now_ns();
    nyxj_value* last = NULL;
    for (int i = 0; i < iterations; i++) {
        nyxj_result r = nyxj_parse_n(data, len);
        if (!r.is_valid) { free(data); return fail("bench", &r.error); }
        if (i == iterations - 1) last = r.root;
        else nyxj_free_value(r.root);
    }
    double dom = (double)(nyxj_now_ns() - t0) / 1e9;

    nyxj_arena* arena = nyxj_arena_create(1 << 20);
    nyxj_parse_options ao = nyxj_parse_options_default();
    ao.arena = arena;
    t0 = nyxj_now_ns();
    for (int i = 0; i < iterations; i++) {
        nyxj_result r = nyxj_parse_ex(data, len, &ao);
        (void)r;
        nyxj_arena_reset(arena);
    }
    double arena_t = (double)(nyxj_now_ns() - t0) / 1e9;
    nyxj_arena_destroy(arena);

    nyxj_view view;
    t0 = nyxj_now_ns();
    for (int i = 0; i < iterations; i++) {
        memset(&view, 0, sizeof(view));
        if (nyxj_view_parse(&view, data, len, NULL, 0, &err)) nyxj_view_free(&view);
    }
    double view_t = (double)(nyxj_now_ns() - t0) / 1e9;

    t0 = nyxj_now_ns();
    for (int i = 0; i < iterations; i++) nyxj_sax_validate(data, len, NULL, &err);
    double sax_t = (double)(nyxj_now_ns() - t0) / 1e9;

    t0 = nyxj_now_ns();
    size_t out_len = 0;
    for (int i = 0; i < iterations; i++) {
        char* s = nyxj_serialize_ex(last, NULL, &out_len);
        free(s);
    }
    double ser_t = (double)(nyxj_now_ns() - t0) / 1e9;

    size_t mp_len = 0;
    uint8_t* mp = nyxj_msgpack_encode(last, &mp_len);
    t0 = nyxj_now_ns();
    for (int i = 0; i < iterations; i++) nyxj_free_value(nyxj_msgpack_decode(mp, mp_len, &err));
    double mp_t = (double)(nyxj_now_ns() - t0) / 1e9;
    double mp_mb = (double)mp_len * iterations / (1024.0 * 1024.0);

    printf("  %-28s %9.1f MB/s\n", "DOM parse (heap)", mb / dom);
    printf("  %-28s %9.1f MB/s\n", "DOM parse (arena)", mb / arena_t);
    printf("  %-28s %9.1f MB/s\n", "Zero-copy view", mb / view_t);
    printf("  %-28s %9.1f MB/s\n", "SAX streaming validation", mb / sax_t);
    printf("  %-28s %9.1f MB/s\n", "Serialize (compact)", (double)out_len * iterations / (1024.0 * 1024.0) / ser_t);
    printf("  %-28s %9.1f MB/s (%.1f%% of JSON size)\n", "MessagePack decode", mp_mb / mp_t, len ? 100.0 * mp_len / len : 0);
    printf("  checksum %016llx\n", (unsigned long long)nyxj_checksum(last));
    free(mp);
    nyxj_free_value(last);
    free(data);
    return 0;
}

static void usage(void) {
    printf("NyxJ %s - JSON toolbox (SIMD: %s)\n\n"
           "Usage: nyxj <command> [options] [file]   (file defaults to stdin, '-' also means stdin)\n\n"
           "Commands:\n"
           "  validate [file]                 Check syntax, report line and column on error\n"
           "  format [--indent N] [file]      Pretty-print\n"
           "  minify [file]                   Compact output\n"
           "  get <path|/pointer> [file]      Extract a value (a.b[0] or /a/b/0)\n"
           "  query '<jsonpath>' [file]       NyxQuery / JSONPath, e.g. '$..book[?(@.price < 10)]'\n"
           "  repair [file]                   Fix almost-JSON (comments, quotes, commas, ...)\n"
           "  schema <schema> <file>          JSON Schema validation\n"
           "  diff [--merge] <from> <to>      JSON Patch (or Merge Patch) between documents\n"
           "  patch <doc> <patch>             Apply a JSON Patch or Merge Patch\n"
           "  msgpack encode|decode [file]    Convert to / from MessagePack (use -o for binary)\n"
           "  gen-struct [--name T] [file]    Generate C structs + mapping tables from a sample\n"
           "  stream [file]                   Constant-memory SAX statistics (NDJSON friendly)\n"
           "  tokens [file]                   Dump the zero-copy token view\n"
           "  gate [--strict] [file]          Run the Guardian sanitizer\n"
           "  bench <file> [--iterations N]   Throughput of every engine mode\n"
           "  version                         Print version and build features\n\n"
           "Global options: -o FILE (write output to FILE), --indent N\n",
           NYXJ_VERSION, nyxj_simd_backend());
}

int main(int argc, char** argv) {
    /* Pull global options out of argv. */
    char** args = malloc(sizeof(char*) * (size_t)(argc + 1));
    if (!args) return 1;
    int n = 0;
    bool merge = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) output_path = argv[++i];
        else if (!strcmp(argv[i], "--indent") && i + 1 < argc) indent_opt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--merge")) merge = true;
        else args[n++] = argv[i];
    }
    int rc = 2;
    const char* cmd = n > 0 ? args[0] : "help";
    int ac = n > 0 ? n - 1 : 0;
    char** av = args + 1;
    if (!strcmp(cmd, "validate")) rc = cmd_validate(ac, av);
    else if (!strcmp(cmd, "format")) rc = cmd_format(ac, av, indent_opt > 0 ? indent_opt : 2);
    else if (!strcmp(cmd, "minify")) rc = cmd_format(ac, av, 0);
    else if (!strcmp(cmd, "get")) rc = cmd_get(ac, av);
    else if (!strcmp(cmd, "query")) rc = cmd_query(ac, av);
    else if (!strcmp(cmd, "repair")) rc = cmd_repair(ac, av);
    else if (!strcmp(cmd, "schema")) rc = cmd_schema(ac, av);
    else if (!strcmp(cmd, "diff")) rc = cmd_diff(ac, av, merge);
    else if (!strcmp(cmd, "patch")) rc = cmd_patch(ac, av);
    else if (!strcmp(cmd, "msgpack")) rc = cmd_msgpack(ac, av);
    else if (!strcmp(cmd, "gen-struct")) rc = cmd_gen_struct(ac, av);
    else if (!strcmp(cmd, "stream")) rc = cmd_stream(ac, av);
    else if (!strcmp(cmd, "tokens")) rc = cmd_tokens(ac, av);
    else if (!strcmp(cmd, "gate")) rc = cmd_gate(ac, av);
    else if (!strcmp(cmd, "bench")) rc = cmd_bench(ac, av);
    else if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) {
        printf("nyxj %s\nsimd: %s\nfeatures: %s\n", nyxj_version(), nyxj_simd_backend(), nyxj_build_features());
        rc = 0;
    } else if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage();
        rc = 0;
    } else {
        fprintf(stderr, "nyxj: unknown command '%s' (see 'nyxj help')\n", cmd);
    }
    free(args);
    return rc;
}
