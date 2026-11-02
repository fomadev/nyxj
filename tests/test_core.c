#include "../include/nyxj.h"
#include "../include/nyxj_log.h"
#include "test.h"

#include <stdlib.h>

static void test_empty_object(void) {
    nyxj_result res = nyxj_parse("{}");
    CHECK(res.is_valid);
    CHECK(res.root && res.root->type == NYXJ_OBJECT && res.root->as.object.count == 0);
    nyxj_free_value(res.root);
}

static void test_scalars(void) {
    nyxj_result r = nyxj_parse("{\"n\": -12.5e2, \"i\": 42, \"t\": true, \"f\": false, \"z\": null, \"s\": \"hi\"}");
    CHECK(r.is_valid);
    CHECK(nyxj_get_number(r.root, "n") == -1250.0);
    CHECK(nyxj_get_int(r.root, "i") == 42);
    CHECK(nyxj_get_bool(r.root, "t") == true);
    CHECK(nyxj_get_bool(r.root, "f") == false);
    CHECK(nyxj_get_member(r.root, "z")->type == NYXJ_NULL);
    CHECK_STR(nyxj_get_string(r.root, "s"), "hi");
    nyxj_free_value(r.root);
}

static void test_escapes_and_unicode(void) {
    nyxj_result r = nyxj_parse("[\"a\\\"b\\\\c\\n\", \"\\u00e9\\u20ac\", \"\\ud83d\\ude00\", \"caf\xc3\xa9\"]");
    CHECK(r.is_valid);
    CHECK_STR(nyxj_array_get(r.root, 0)->as.string, "a\"b\\c\n");
    CHECK_STR(nyxj_array_get(r.root, 1)->as.string, "\xc3\xa9\xe2\x82\xac");
    CHECK_STR(nyxj_array_get(r.root, 2)->as.string, "\xf0\x9f\x98\x80");
    CHECK_STR(nyxj_array_get(r.root, 3)->as.string, "caf\xc3\xa9");
    char* out = nyxj_serialize(r.root);
    CHECK_STR(out, "[\"a\\\"b\\\\c\\n\",\"\xc3\xa9\xe2\x82\xac\",\"\xf0\x9f\x98\x80\",\"caf\xc3\xa9\"]");
    free(out);
    nyxj_serialize_options o = {0, false, true};
    out = nyxj_serialize_ex(r.root, &o, NULL);
    CHECK(out && strstr(out, "\\ud83d\\ude00") != NULL);
    free(out);
    nyxj_free_value(r.root);
}

static void test_errors_have_positions(void) {
    nyxj_result r = nyxj_parse("{\n  \"a\": 1,\n  \"b\": tru\n}");
    CHECK(!r.is_valid);
    CHECK(r.root == NULL);
    CHECK(r.error.code == NYXJ_ERR_INVALID_LITERAL);
    CHECK(r.error.line == 3);
    CHECK(r.error.column == 8);
    CHECK(r.error_msg != NULL);

    r = nyxj_parse("[1, 2");
    CHECK(r.error.code == NYXJ_ERR_UNEXPECTED_EOF);
    r = nyxj_parse("{\"a\":1} x");
    CHECK(r.error.code == NYXJ_ERR_TRAILING_DATA);
    r = nyxj_parse("[01]");
    CHECK(!r.is_valid);
    r = nyxj_parse("\"bad \\x escape\"");
    CHECK(r.error.code == NYXJ_ERR_INVALID_ESCAPE);
    r = nyxj_parse("\"\xff\"");
    CHECK(r.error.code == NYXJ_ERR_INVALID_UTF8);
    r = nyxj_parse("\"tab\there\"");
    CHECK(r.error.code == NYXJ_ERR_CONTROL_CHAR);
    r = nyxj_parse("\"\\ud800\"");
    CHECK(r.error.code == NYXJ_ERR_INVALID_UNICODE);
    r = nyxj_parse(NULL);
    CHECK(r.error.code == NYXJ_ERR_NULL_INPUT);
    CHECK_STR(nyxj_error_name(NYXJ_ERR_DEPTH_LIMIT), "DEPTH_LIMIT");
}

static void test_jdos_limits(void) {
    char deep[2100];
    for (int i = 0; i < 1000; i++) deep[i] = '[';
    for (int i = 0; i < 1000; i++) deep[1000 + i] = ']';
    deep[2000] = '\0';
    nyxj_result r = nyxj_parse(deep);
    CHECK(!r.is_valid && r.error.code == NYXJ_ERR_DEPTH_LIMIT);

    nyxj_parse_options o = nyxj_parse_options_default();
    o.max_depth = 2000;
    r = nyxj_parse_ex(deep, strlen(deep), &o);
    CHECK(r.is_valid);
    nyxj_free_value(r.root);

    o = nyxj_parse_options_default();
    o.max_elements = 3;
    r = nyxj_parse_ex("[1,2,3,4]", 9, &o);
    CHECK(r.error.code == NYXJ_ERR_ELEMENT_LIMIT);
    o.max_elements = 0;
    o.max_string_length = 4;
    r = nyxj_parse_ex("\"hello\"", 7, &o);
    CHECK(r.error.code == NYXJ_ERR_STRING_LIMIT);
    o.max_string_length = 0;
    o.max_input_size = 4;
    r = nyxj_parse_ex("[1,2,3]", 7, &o);
    CHECK(r.error.code == NYXJ_ERR_SIZE_LIMIT);
    o.max_input_size = 0;
    o.flags = NYXJ_PARSE_REJECT_DUPLICATE_KEYS;
    r = nyxj_parse_ex("{\"a\":1,\"a\":2}", 13, &o);
    CHECK(r.error.code == NYXJ_ERR_DUPLICATE_KEY);
}

static void test_relaxed_mode(void) {
    const char* txt = "{ // comment\n \"a\": [1, 2,], /* block */ \"b\": 2, }";
    nyxj_result r = nyxj_parse(txt);
    CHECK(!r.is_valid);
    nyxj_parse_options o = nyxj_parse_options_default();
    o.flags = NYXJ_PARSE_RELAXED;
    r = nyxj_parse_ex(txt, strlen(txt), &o);
    CHECK(r.is_valid);
    CHECK(nyxj_size(nyxj_get_member(r.root, "a")) == 2);
    nyxj_free_value(r.root);
}

static void test_builder_and_path(void) {
    nyxj_value* root = nyxj_new_object();
    nyxj_value* user = nyxj_new_object();
    nyxj_object_set_string(user, "name", "Ada");
    nyxj_object_set_number(user, "age", 36);
    nyxj_value* tags = nyxj_new_array();
    for (int i = 0; i < 10; i++) nyxj_array_push(tags, nyxj_new_number(i));
    nyxj_object_set(user, "tags", tags);
    nyxj_object_set(root, "user", user);
    CHECK_STR(nyxj_path_string(root, "user.name", NULL), "Ada");
    CHECK(nyxj_path_number(root, "user.tags[9]", -1) == 9);
    CHECK(nyxj_path_number(root, "user.tags[-1]", -1) == 9);
    CHECK(nyxj_get_path(root, "user.missing") == NULL);
    CHECK(nyxj_path_bool(root, "user.name", true) == true);
    nyxj_object_set_string(user, "name", "Grace");
    CHECK_STR(nyxj_path_string(root, "$.user.name", NULL), "Grace");
    CHECK(nyxj_array_insert(tags, 0, nyxj_new_string("first")));
    CHECK(nyxj_size(tags) == 11);
    CHECK(nyxj_array_remove(tags, 0));
    CHECK(nyxj_object_remove(user, "age"));
    CHECK(nyxj_get_member(user, "age") == NULL);

    nyxj_value* copy = nyxj_clone(root);
    CHECK(nyxj_equals(root, copy));
    nyxj_object_set_bool(copy, "extra", true);
    CHECK(!nyxj_equals(root, copy));
    nyxj_free_value(copy);
    nyxj_free_value(root);
}

static void test_serializer(void) {
    nyxj_result r = nyxj_parse("{\"b\":[1,2.5,1e300,-0.1],\"a\":{\"x\":null},\"e\":[],\"o\":{}}");
    CHECK(r.is_valid);
    char* s = nyxj_serialize(r.root);
    CHECK_STR(s, "{\"b\":[1,2.5,1e+300,-0.1],\"a\":{\"x\":null},\"e\":[],\"o\":{}}");
    free(s);
    nyxj_serialize_options o = {0, true, false};
    s = nyxj_serialize_ex(r.root, &o, NULL);
    CHECK(s && strncmp(s, "{\"a\":", 5) == 0);
    free(s);
    s = nyxj_serialize_pretty(r.root, 2);
    CHECK(s && strstr(s, "\n  \"a\": {\n    \"x\": null\n  }") != NULL);
    /* Round trip */
    nyxj_result back = nyxj_parse(s);
    CHECK(back.is_valid && nyxj_equals(back.root, r.root));
    nyxj_free_value(back.root);
    free(s);
    nyxj_free_value(r.root);

    nyxj_value* n = nyxj_new_number(0.1 + 0.2);
    s = nyxj_serialize(n);
    nyxj_result rt = nyxj_parse(s);
    CHECK(rt.is_valid && rt.root->as.number == 0.1 + 0.2);
    nyxj_free_value(rt.root);
    free(s);
    nyxj_free_value(n);
}

static void test_arena(void) {
    nyxj_arena* a = nyxj_arena_create(256);
    nyxj_parse_options o = nyxj_parse_options_default();
    o.arena = a;
    const char* doc = "{\"list\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20],\"s\":\"text\"}";
    nyxj_result r = nyxj_parse_ex(doc, strlen(doc), &o);
    CHECK(r.is_valid);
    CHECK(nyxj_path_number(r.root, "list[19]", 0) == 20);
    CHECK(nyxj_arena_used(a) > 0);
    nyxj_value* extra = nyxj_new_null();
    CHECK(!nyxj_array_push(nyxj_get_member(r.root, "list"), extra)); /* Arena trees are read-only */
    nyxj_free_value(extra);
    nyxj_free_value(r.root); /* No-op for arena trees */
    nyxj_arena_reset(a);
    CHECK(nyxj_arena_used(a) == 0);
    nyxj_arena_destroy(a);
}

static int log_hits = 0;
static void sink(nyxj_log_level l, const char* m, const char* msg, void* u) {
    (void)l; (void)m; (void)u;
    if (strstr(msg, "hello 7")) log_hits++;
}

static void test_logging(void) {
    nyxj_log_level prev = nyxj_log_get_level();
    nyxj_log_set_sink(sink, NULL);
    nyxj_log_set_level(NYXJ_LOG_INFO);
    NYXJ_DEBUG("test", "hello %d", 7); /* Filtered out */
    NYXJ_INFO("test", "hello %d", 7);
    nyxj_log_set_sink(NULL, NULL);
    nyxj_log_set_level(prev);
    CHECK(log_hits == 1);
    nyxj_log_level lv;
    CHECK(nyxj_log_level_parse("Warn", &lv) && lv == NYXJ_LOG_WARN);
    CHECK(!nyxj_log_level_parse("loud", &lv));
}

static void test_simd_long_strings(void) {
    /* Long strings exercise the 16-byte SIMD paths, escapes land on block edges. */
    char buf[600];
    size_t k = 0;
    buf[k++] = '"';
    for (int i = 0; i < 500; i++) buf[k++] = (char)('a' + i % 26);
    buf[k++] = '\\';
    buf[k++] = 'n';
    buf[k++] = '"';
    buf[k] = '\0';
    nyxj_result r = nyxj_parse(buf);
    CHECK(r.is_valid && strlen(r.root->as.string) == 501 && r.root->as.string[500] == '\n');
    nyxj_free_value(r.root);
    char ws[200];
    memset(ws, ' ', sizeof(ws));
    memcpy(ws + 150, "[true]", 6);
    ws[156] = '\0';
    r = nyxj_parse(ws);
    CHECK(r.is_valid);
    nyxj_free_value(r.root);
}

void suite_core(void) {
    RUN(test_empty_object);
    RUN(test_scalars);
    RUN(test_escapes_and_unicode);
    RUN(test_errors_have_positions);
    RUN(test_jdos_limits);
    RUN(test_relaxed_mode);
    RUN(test_builder_and_path);
    RUN(test_serializer);
    RUN(test_arena);
    RUN(test_logging);
    RUN(test_simd_long_strings);
}
