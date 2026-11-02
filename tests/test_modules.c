#include "../include/nyxj.h"
#include "../include/nyxj_gate.h"
#include "../include/nyxj_map.h"
#include "../include/nyxj_msgpack.h"
#include "../include/nyxj_patch.h"
#include "../include/nyxj_query.h"
#include "../include/nyxj_reliability.h"
#include "../include/nyxj_repair.h"
#include "../include/nyxj_sax.h"
#include "../include/nyxj_schema.h"
#include "../include/nyxj_view.h"
#include "test.h"

#include <stdlib.h>

/* --- Struct mapping --- */

typedef struct {
    char city[16];
    int zip;
} Address;

typedef struct {
    int id;
    char* name;
    double score;
    bool active;
    int64_t big;
    Address address;
    nyxj_value* tags;
} User;

static const nyxj_field address_fields[] = {
    NYXJ_FIELD_STRBUF(Address, city, "city", true),
    NYXJ_FIELD_INT(Address, zip, "zip", false),
};

static const nyxj_field user_fields[] = {
    NYXJ_FIELD_INT(User, id, "id", true),
    NYXJ_FIELD_STRING(User, name, "name", true),
    NYXJ_FIELD_DOUBLE(User, score, "score", false),
    NYXJ_FIELD_BOOL(User, active, "active", false),
    NYXJ_FIELD_INT64(User, big, "big", false),
    NYXJ_FIELD_OBJECT(User, address, "address", false, address_fields),
    NYXJ_FIELD_VALUE(User, tags, "tags", false),
};

static void test_map(void) {
    User u;
    memset(&u, 0, sizeof(u));
    nyxj_error err;
    const char* json = "{\"id\":7,\"name\":\"Ada\",\"score\":9.5,\"active\":true,\"big\":9007199254740991,"
                       "\"address\":{\"city\":\"Kinshasa\",\"zip\":243},\"tags\":[\"a\",\"b\"]}";
    CHECK(nyxj_map_parse(json, user_fields, NYXJ_FIELDS_COUNT(user_fields), &u, &err));
    CHECK(u.id == 7 && u.score == 9.5 && u.active && u.big == 9007199254740991LL);
    CHECK_STR(u.name, "Ada");
    CHECK_STR(u.address.city, "Kinshasa");
    CHECK(u.address.zip == 243);
    CHECK(u.tags && u.tags->as.array.count == 2);
    char* back = nyxj_struct_to_json(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields), 0);
    nyxj_result a = nyxj_parse(json), b = nyxj_parse(back);
    CHECK(a.is_valid && b.is_valid && nyxj_equals(a.root, b.root));
    nyxj_free_value(a.root);
    nyxj_free_value(b.root);
    free(back);
    nyxj_map_free(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields));
    CHECK(u.name == NULL && u.tags == NULL);

    memset(&u, 0, sizeof(u));
    CHECK(!nyxj_map_parse("{\"id\":1}", user_fields, NYXJ_FIELDS_COUNT(user_fields), &u, &err));
    CHECK(err.code == NYXJ_ERR_MISSING_FIELD && strstr(err.message, "name"));
    CHECK(!nyxj_map_parse("{\"id\":1.5,\"name\":\"x\"}", user_fields, NYXJ_FIELDS_COUNT(user_fields), &u, &err));
    CHECK(err.code == NYXJ_ERR_TYPE_MISMATCH);
    nyxj_map_free(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields));
    CHECK(!nyxj_map_parse("{\"id\":1,\"name\":\"x\",\"address\":{\"city\":\"a very long city name\"}}", user_fields,
                          NYXJ_FIELDS_COUNT(user_fields), &u, &err));
    CHECK(err.code == NYXJ_ERR_STRING_LIMIT && strstr(err.message, "address.city"));
    nyxj_map_free(&u, user_fields, NYXJ_FIELDS_COUNT(user_fields));

    nyxj_result s = nyxj_parse("{\"id\":1,\"profile\":{\"nick\":\"x\",\"ratio\":0.5},\"list\":[1]}");
    char* code = nyxj_generate_struct(s.root, "Account");
    CHECK(code && strstr(code, "} AccountProfile;") && strstr(code, "NYXJ_FIELD_OBJECT(Account, profile, \"profile\", true, AccountProfile_fields)"));
    CHECK(code && strstr(code, "double ratio;") && strstr(code, "nyxj_value* list;"));
    free(code);
    nyxj_free_value(s.root);
}

/* --- Schema --- */

static void test_schema(void) {
    const char* schema =
        "{\"type\":\"object\",\"required\":[\"name\",\"age\"],\"additionalProperties\":false,"
        "\"properties\":{\"name\":{\"type\":\"string\",\"minLength\":2},"
        "\"age\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":150},"
        "\"email\":{\"type\":\"string\",\"format\":\"email\"},"
        "\"tags\":{\"type\":\"array\",\"items\":{\"type\":\"string\"},\"uniqueItems\":true,\"maxItems\":3},"
        "\"role\":{\"enum\":[\"admin\",\"user\"]},"
        "\"id\":{\"$ref\":\"#/$defs/uuid\"}},"
        "\"$defs\":{\"uuid\":{\"type\":\"string\",\"format\":\"uuid\"}}}";
    nyxj_schema_report rep;
    nyxj_error err;
    CHECK(nyxj_schema_validate_json(schema, "{\"name\":\"Ada\",\"age\":36,\"email\":\"ada@example.org\",\"tags\":[\"x\"],"
                                            "\"role\":\"admin\",\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}", &rep, &err));
    CHECK(rep.total == 0);
    CHECK(!nyxj_schema_validate_json(schema, "{\"name\":\"A\",\"age\":-1.5,\"email\":\"nope\",\"tags\":[\"x\",\"x\"],"
                                             "\"role\":\"root\",\"id\":\"bad\",\"extra\":1}", &rep, &err));
    CHECK(rep.total >= 7);
    bool saw_min = false, saw_extra = false;
    for (int i = 0; i < rep.count; i++) {
        if (!strcmp(rep.issues[i].keyword, "minLength") && !strcmp(rep.issues[i].path, "$.name")) saw_min = true;
        if (!strcmp(rep.issues[i].keyword, "additionalProperties")) saw_extra = true;
    }
    CHECK(saw_min && saw_extra);
    CHECK(!nyxj_schema_validate_json(schema, "{\"name\":\"Ada\"}", &rep, &err));
    CHECK(rep.count == 1 && !strcmp(rep.issues[0].keyword, "required"));
    CHECK(nyxj_schema_validate_json("{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"number\"}]}", "3", NULL, &err));
    CHECK(!nyxj_schema_validate_json("{\"oneOf\":[{\"type\":\"number\"},{\"type\":\"integer\"}]}", "3", NULL, &err));
    CHECK(nyxj_schema_validate_json("{\"not\":{\"type\":\"null\"}}", "1", NULL, &err));
    CHECK(nyxj_schema_validate_json("{\"if\":{\"minimum\":10},\"then\":{\"multipleOf\":5},\"else\":{\"maximum\":3}}", "15", NULL, &err));
    CHECK(!nyxj_schema_validate_json("{\"if\":{\"minimum\":10},\"then\":{\"multipleOf\":5},\"else\":{\"maximum\":3}}", "7", NULL, &err));
}

/* --- JSONPath --- */

static const char* store =
    "{\"store\":{\"book\":["
    "{\"category\":\"fiction\",\"author\":\"Camus\",\"title\":\"L'Etranger\",\"price\":8.95},"
    "{\"category\":\"fiction\",\"author\":\"Tolkien\",\"title\":\"LOTR\",\"price\":22.99,\"isbn\":\"0-395\"},"
    "{\"category\":\"science\",\"author\":\"Hawking\",\"title\":\"A Brief History\",\"price\":12.5}],"
    "\"bicycle\":{\"color\":\"red\",\"price\":19.95}}}";

static void test_query(void) {
    nyxj_result r = nyxj_parse(store);
    nyxj_error err;
    CHECK(nyxj_query_count(r.root, "$.store.book[*].author", &err) == 3);
    CHECK(nyxj_query_count(r.root, "$..price", &err) == 4);
    const nyxj_value* first = nyxj_query_first(r.root, "$.store.book[?(@.price < 10)].title", &err);
    CHECK(first && !strcmp(first->as.string, "L'Etranger"));
    CHECK(nyxj_query_count(r.root, "$..book[?(@.isbn)]", &err) == 1);
    CHECK(nyxj_query_count(r.root, "$..book[?(@.category == 'fiction' && @.price > 10)]", &err) == 1);
    CHECK(nyxj_query_count(r.root, "$..book[?(@.author == 'Camus' || @.author == 'Hawking')]", &err) == 2);
    CHECK(nyxj_query_count(r.root, "$..book[?(!(@.price > 10))]", &err) == 1);
    first = nyxj_query_first(r.root, "$.store.book[-1].author", &err);
    CHECK(first && !strcmp(first->as.string, "Hawking"));
    CHECK(nyxj_query_count(r.root, "$.store.book[0:2]", &err) == 2);
    first = nyxj_query_first(r.root, "$.store.book[::-1].author", &err);
    CHECK(first && !strcmp(first->as.string, "Hawking"));
    CHECK(nyxj_query_count(r.root, "$['store']['bicycle']['color']", &err) == 1);
    CHECK(nyxj_query_count(r.root, "$.store.book[0,2].title", &err) == 2);
    CHECK(nyxj_query_count(r.root, "$.store.*", &err) == 2);
    CHECK(nyxj_query_count(r.root, "$..book[?(@.price > $.store.bicycle.price)]", &err) == 1);
    nyxj_value* all = nyxj_query(r.root, "$..author", &err);
    CHECK(all && all->as.array.count == 3);
    nyxj_free_value(all);
    CHECK(nyxj_query(r.root, "$.store[", &err) == NULL && err.code == NYXJ_ERR_INVALID_QUERY);
    nyxj_free_value(r.root);
}

/* --- Patch / diff --- */

static void test_patch(void) {
    nyxj_result a = nyxj_parse("{\"name\":\"Ada\",\"langs\":[\"c\",\"go\"],\"meta\":{\"v\":1,\"old\":true},\"a/b\":1}");
    nyxj_result b = nyxj_parse("{\"name\":\"Ada L.\",\"langs\":[\"c\",\"go\",\"rust\"],\"meta\":{\"v\":2},\"new\":null,\"a/b\":1}");
    CHECK(nyxj_pointer_get(a.root, "/langs/1") && !strcmp(nyxj_pointer_get(a.root, "/langs/1")->as.string, "go"));
    CHECK(nyxj_pointer_get(a.root, "/a~1b") != NULL);
    CHECK(nyxj_pointer_get(a.root, "") == a.root);
    nyxj_value* patch = nyxj_diff(a.root, b.root);
    CHECK(patch && patch->as.array.count == 5);
    nyxj_value* doc = nyxj_clone(a.root);
    nyxj_error err;
    CHECK(nyxj_patch_apply(&doc, patch, &err));
    CHECK(nyxj_equals(doc, b.root));
    nyxj_free_value(doc);
    nyxj_free_value(patch);

    nyxj_value* md = nyxj_merge_diff(a.root, b.root);
    nyxj_value* merged = nyxj_merge_patch(nyxj_clone(a.root), md);
    CHECK(nyxj_equals(nyxj_get_member(merged, "meta"), nyxj_get_member(b.root, "meta")));
    CHECK(nyxj_get_member(merged, "langs")->as.array.count == 3);
    nyxj_free_value(merged);
    nyxj_free_value(md);

    nyxj_result ops = nyxj_parse("[{\"op\":\"test\",\"path\":\"/name\",\"value\":\"Ada\"},"
                                 "{\"op\":\"move\",\"from\":\"/meta/v\",\"path\":\"/version\"},"
                                 "{\"op\":\"copy\",\"from\":\"/langs/0\",\"path\":\"/langs/-\"},"
                                 "{\"op\":\"remove\",\"path\":\"/meta/old\"}]");
    doc = nyxj_clone(a.root);
    CHECK(nyxj_patch_apply(&doc, ops.root, &err));
    CHECK(nyxj_get_number(doc, "version") == 1);
    CHECK(nyxj_get_member(doc, "langs")->as.array.count == 3);
    CHECK(nyxj_get_member(nyxj_get_member(doc, "meta"), "old") == NULL);
    nyxj_free_value(ops.root);

    ops = nyxj_parse("[{\"op\":\"replace\",\"path\":\"/name\",\"value\":\"X\"},{\"op\":\"test\",\"path\":\"/name\",\"value\":\"Y\"}]");
    nyxj_value* before = nyxj_clone(doc);
    CHECK(!nyxj_patch_apply(&doc, ops.root, &err) && err.code == NYXJ_ERR_TEST_FAILED);
    CHECK(nyxj_equals(before, doc)); /* Atomic: unchanged on failure */
    nyxj_free_value(before);
    nyxj_free_value(ops.root);
    nyxj_free_value(doc);
    nyxj_free_value(a.root);
    nyxj_free_value(b.root);
}

/* --- MessagePack --- */

static void test_msgpack(void) {
    const char* json = "{\"int\":1,\"neg\":-100000,\"big\":4294967296,\"f\":0.5,\"d\":3.14159,\"s\":\"hello\","
                       "\"long\":\"0123456789012345678901234567890123456789\",\"arr\":[true,false,null],\"obj\":{}}";
    size_t n;
    nyxj_error err;
    uint8_t* bin = nyxj_json_to_msgpack(json, strlen(json), &n, &err);
    CHECK(bin != NULL && n < strlen(json));
    char* back = nyxj_msgpack_to_json(bin, n, &err);
    nyxj_result a = nyxj_parse(json), b = nyxj_parse(back);
    CHECK(b.is_valid && nyxj_equals(a.root, b.root));
    nyxj_free_value(a.root);
    nyxj_free_value(b.root);
    free(back);
    CHECK(nyxj_msgpack_decode(bin, n - 1, &err) == NULL && err.code == NYXJ_ERR_MSGPACK);
    free(bin);
    const uint8_t fixmap[] = {0x81, 0xA1, 'a', 0x93, 0x01, 0xFF, 0xCB, 0x40, 0x09, 0x21, 0xFB, 0x54, 0x44, 0x2D, 0x18};
    nyxj_value* v = nyxj_msgpack_decode(fixmap, sizeof(fixmap), &err);
    CHECK(v && nyxj_path_number(v, "a[1]", 0) == -1);
    CHECK(v && nyxj_path_number(v, "a[2]", 0) > 3.14159 && nyxj_path_number(v, "a[2]", 0) < 3.1416);
    nyxj_free_value(v);
}

/* --- Zero-copy view --- */

static void test_view(void) {
    const char* json = "{\"user\":{\"name\":\"Ada\",\"langs\":[\"c\",\"go\"],\"esc\":\"a\\nb\"},\"n\":42.5,\"ok\":true}";
    nyxj_tok toks[32];
    nyxj_view v;
    memset(&v, 0, sizeof(v));
    nyxj_error err;
    CHECK(nyxj_view_parse(&v, json, strlen(json), toks, 32, &err)); /* Zero heap allocation */
    size_t name = nyxj_view_path(&v, "user.name");
    CHECK(name != NYXJ_VIEW_NONE && nyxj_view_eq(&v, name, "Ada"));
    size_t len;
    const char* raw = nyxj_view_raw(&v, name, &len);
    CHECK(raw >= json && raw < json + strlen(json) && len == 3); /* Points into the original buffer */
    CHECK(nyxj_view_number(&v, nyxj_view_path(&v, "n")) == 42.5);
    CHECK(nyxj_view_bool(&v, nyxj_view_path(&v, "ok")));
    size_t go = nyxj_view_path(&v, "user.langs[1]");
    CHECK(nyxj_view_eq(&v, go, "go"));
    char buf[16];
    CHECK(nyxj_view_string(&v, nyxj_view_path(&v, "user.esc"), buf, sizeof(buf)) == 3 && !strcmp(buf, "a\nb"));
    CHECK(nyxj_view_eq(&v, nyxj_view_path(&v, "user.esc"), "a\nb"));
    nyxj_value* sub = nyxj_view_to_value(&v, nyxj_view_path(&v, "user"));
    CHECK(sub && nyxj_size(nyxj_get_member(sub, "langs")) == 2);
    nyxj_free_value(sub);
    nyxj_tok tiny[3];
    memset(&v, 0, sizeof(v));
    CHECK(!nyxj_view_parse(&v, json, strlen(json), tiny, 3, &err) && err.code == NYXJ_ERR_ELEMENT_LIMIT);
    memset(&v, 0, sizeof(v));
    CHECK(nyxj_view_parse(&v, json, strlen(json), NULL, 0, &err) && v.count > 3);
    nyxj_view_free(&v);
    memset(&v, 0, sizeof(v));
    CHECK(!nyxj_view_parse(&v, "{\"a\":}", 6, NULL, 0, &err));
}

/* --- SAX streaming --- */

typedef struct { int objects, numbers, docs; double sum; } sax_counts;
static bool c_obj(void* u) { ((sax_counts*)u)->objects++; return true; }
static bool c_num(void* u, double v, const char* r, size_t n) { (void)r; (void)n; ((sax_counts*)u)->numbers++; ((sax_counts*)u)->sum += v; return true; }
static bool c_doc(void* u) { ((sax_counts*)u)->docs++; return true; }

static void test_sax(void) {
    sax_counts c;
    memset(&c, 0, sizeof(c));
    nyxj_sax_handler h;
    memset(&h, 0, sizeof(h));
    h.on_object_start = c_obj;
    h.on_number = c_num;
    h.on_document_end = c_doc;
    nyxj_sax_options o;
    memset(&o, 0, sizeof(o));
    o.multi_document = true;
    o.max_token_size = 256;
    nyxj_sax* s = nyxj_sax_create(&h, &c, &o);
    const char* stream = "{\"sensor\":\"t1\",\"v\":21.5}\n{\"sensor\":\"t2\",\"v\":-3}\n{\"s\":\"\\u00e9\\ud83d\\ude00\",\"v\":1e2}\n";
    /* Feed one byte at a time: every token crosses chunk boundaries. */
    bool ok = true;
    for (size_t i = 0; ok && stream[i]; i++) ok = nyxj_sax_feed(s, stream + i, 1);
    CHECK(ok && nyxj_sax_finish(s));
    CHECK(c.docs == 3 && c.objects == 3 && c.numbers == 3 && c.sum == 118.5);
    CHECK(nyxj_sax_memory(s) < 4096); /* Constant footprint */
    nyxj_sax_destroy(s);

    nyxj_error err;
    CHECK(nyxj_sax_validate("[1,[2,{\"a\":[true,null]}]]", 25, NULL, &err));
    CHECK(!nyxj_sax_validate("[1,2", 4, NULL, &err) && err.code == NYXJ_ERR_UNEXPECTED_EOF);
    CHECK(!nyxj_sax_validate("{\"a\" 1}", 7, NULL, &err) && err.code == NYXJ_ERR_UNEXPECTED_CHAR);
    CHECK(!nyxj_sax_validate("[\"\xc3\x28\"]", 6, NULL, &err) && err.code == NYXJ_ERR_INVALID_UTF8);
    CHECK(!nyxj_sax_validate("{} {}", 5, NULL, &err) && err.code == NYXJ_ERR_TRAILING_DATA);
    memset(&o, 0, sizeof(o));
    o.max_depth = 3;
    CHECK(!nyxj_sax_validate("[[[[1]]]]", 9, &o, &err) && err.code == NYXJ_ERR_DEPTH_LIMIT);
}

/* --- Repair --- */

static void check_repair(const char* in, const char* expected, unsigned kind) {
    nyxj_repair_report rep;
    size_t n;
    char* out = nyxj_repair(in, strlen(in), &n, &rep);
    CHECK_STR(out, expected);
    if (!out || strcmp(out, expected)) printf("      repair(%s) -> %s\n", in, out ? out : "(null)");
    CHECK(kind == 0 || (rep.kinds & kind));
    free(out);
}

static void test_repair(void) {
    check_repair("{a: 1, b: 'two', c: [1, 2,],}", "{\"a\":1,\"b\":\"two\",\"c\":[1,2]}", NYXJ_FIX_UNQUOTED_KEY);
    check_repair("{\"a\": 1 \"b\": 2}", "{\"a\":1,\"b\":2}", NYXJ_FIX_MISSING_COMMA);
    check_repair("// config\n{\"debug\": True, \"x\": None, /* c */ \"y\": undefined}", "{\"debug\":true,\"x\":null,\"y\":null}",
                 NYXJ_FIX_COMMENT | NYXJ_FIX_LITERAL);
    check_repair("{\"items\": [1, 2, {\"deep\": \"unterminated", "{\"items\":[1,2,{\"deep\":\"unterminated\"}]}", NYXJ_FIX_UNCLOSED);
    check_repair("[+1, .5, 007, 0x1F, 5.]", "[1,0.5,7,31,5]", NYXJ_FIX_NUMBER);
    check_repair("```json\n{\"ok\": true}\n```", "{\"ok\":true}", NYXJ_FIX_CODE_FENCE);
    check_repair("{\"a\" \"b\"}", "{\"a\":\"b\"}", NYXJ_FIX_MISSING_COLON);
    check_repair("{\"a\": }", "{\"a\":null}", NYXJ_FIX_MISSING_VALUE);
    check_repair("[1, 2}", "[1,2]", NYXJ_FIX_BRACKET);
    check_repair("{\"line\": \"a\nb\"}", "{\"line\":\"a\\nb\"}", NYXJ_FIX_ESCAPE);
    check_repair("{\"status\": ok}", "{\"status\":\"ok\"}", NYXJ_FIX_QUOTES);
    check_repair("{\"valid\": 1}", "{\"valid\":1}", 0);
    nyxj_repair_report rep;
    nyxj_result r = nyxj_parse_lenient("{name: 'NyxJ', tags: ['fast' 'small']}", 37, &rep);
    CHECK(r.is_valid && nyxj_size(nyxj_get_member(r.root, "tags")) == 2 && rep.fixes > 0);
    nyxj_free_value(r.root);
    CHECK(nyxj_repair("   ", 3, NULL, NULL) == NULL);
}

/* --- Guardian --- */

static void test_gate(void) {
    nyxj_gate_policy p = nyxj_gate_policy_strict();
    nyxj_gate_verdict v;
    const char* ok = "{\"user\":\"ada\",\"n\":[1,2,3]}";
    CHECK(nyxj_gate_check(ok, strlen(ok), &p, &v) && v.allowed);
    CHECK(!nyxj_gate_check("[1,2]", 5, &p, &v) && !strcmp(v.reason, "root_type"));
    char bomb[64];
    memset(bomb, '[', 30);
    memset(bomb + 30, ']', 30);
    bomb[0] = '{';
    p.require_object_root = false;
    CHECK(!nyxj_gate_check(bomb + 1, 59, &p, &v) && !strcmp(v.reason, "depth_limit"));
    CHECK(!nyxj_gate_check("{\"a\":\"\xc0\xaf\"}", 10, &p, &v) && !strcmp(v.reason, "invalid_utf8"));
    CHECK(!nyxj_gate_check("{\"a\":1}\0x", 9, &p, &v) && !strcmp(v.reason, "control_char"));
    p.max_size = 8;
    CHECK(!nyxj_gate_check(ok, strlen(ok), &p, &v) && !strcmp(v.reason, "size_limit"));
    CHECK(!nyxj_gate_check("{\"a\":", 5, NULL, &v) && !strcmp(v.reason, "empty_or_truncated"));
    CHECK(nyxj_gate_check("\xEF\xBB\xBF{}", 5, NULL, &v) && v.body_offset == 3);
}

/* --- Reliability --- */

static void test_reliability(void) {
    const char* json = "{\"patient\":\"x\",\"bpm\":[72,75,71],\"ok\":true}";
    nyxj_result r = nyxj_parse_verified(json, strlen(json), NULL);
    CHECK(r.is_valid);
    nyxj_result plain = nyxj_parse(json);
    CHECK(nyxj_checksum(r.root) == nyxj_checksum(plain.root));
    nyxj_result bad = nyxj_parse_verified("{\"a\":", 5, NULL);
    CHECK(!bad.is_valid && bad.error.code != NYXJ_ERR_INTEGRITY);

    nyxj_tmr* t = nyxj_tmr_create(r.root);
    bool repaired;
    CHECK(nyxj_equals(nyxj_tmr_read(t, &repaired), plain.root) && !repaired);
    nyxj_path_number(t->replica[1], "bpm[0]", 0);
    nyxj_array_get(nyxj_get_member(t->replica[1], "bpm"), 0)->as.number = 999; /* Simulated bit-flip */
    const nyxj_value* voted = nyxj_tmr_read(t, &repaired);
    CHECK(repaired && nyxj_equals(voted, plain.root) && nyxj_equals(t->replica[1], plain.root) && t->repairs == 1);
    nyxj_get_member(t->replica[0], "ok")->as.boolean = false;
    nyxj_object_set_null(t->replica[2], "patient");
    CHECK(nyxj_tmr_read(t, &repaired) == NULL); /* No majority left */
    nyxj_tmr_free(t);
    nyxj_free_value(r.root);
    nyxj_free_value(plain.root);
}

void suite_modules(void) {
    RUN(test_map);
    RUN(test_schema);
    RUN(test_query);
    RUN(test_patch);
    RUN(test_msgpack);
    RUN(test_view);
    RUN(test_sax);
    RUN(test_repair);
    RUN(test_gate);
    RUN(test_reliability);
}
