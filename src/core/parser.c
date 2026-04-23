#include "../../include/nyxj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Lexer & Parser Internal Logic (Scanning) --- */

typedef enum {
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_STRING, TOKEN_NUMBER, TOKEN_COLON, TOKEN_COMMA,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL, TOKEN_EOF, TOKEN_ERROR
} nyxj_token_type;

typedef struct {
    nyxj_token_type type;
    const char* start;
    int length;
} nyxj_token;

typedef struct { const char *start; const char *current; } Scanner;
typedef struct { nyxj_token current; nyxj_token previous; bool had_error; } Parser;

Scanner scanner;
Parser parser;

/* --- Forward Declarations --- */
static nyxj_value* parse_value();
static nyxj_value* parse_object();
static nyxj_value* parse_array();

/* --- Lexer Implementation --- */

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

static void skip_whitespace() {
    for (;;) {
        char c = *scanner.current;
        switch (c) {
            case ' ': case '\r': case '\t': case '\n': scanner.current++; break;
            default: return;
        }
    }
}

static nyxj_token_type identifier_type(const char* start) {
    int len = (int)(scanner.current - start);
    if (len == 4 && memcmp(start, "true", 4) == 0) return TOKEN_TRUE;
    if (len == 5 && memcmp(start, "false", 5) == 0) return TOKEN_FALSE;
    if (len == 4 && memcmp(start, "null", 4) == 0) return TOKEN_NULL;
    return TOKEN_ERROR;
}

nyxj_token next_token() {
    skip_whitespace();
    nyxj_token token;
    token.start = scanner.current;
    if (*scanner.current == '\0') { token.type = TOKEN_EOF; token.length = 0; return token; }
    char c = *scanner.current++;
    if (is_alpha(c)) {
        while (is_alpha(*scanner.current)) scanner.current++;
        token.type = identifier_type(token.start);
        token.length = (int)(scanner.current - token.start);
        return token;
    }
    if (is_digit(c) || c == '-') {
        while (is_digit(*scanner.current)) scanner.current++;
        if (*scanner.current == '.' && is_digit(scanner.current[1])) {
            scanner.current++;
            while (is_digit(*scanner.current)) scanner.current++;
        }
        token.type = TOKEN_NUMBER;
        token.length = (int)(scanner.current - token.start);
        return token;
    }
    switch (c) {
        case '{': token.type = TOKEN_LBRACE; token.length = 1; return token;
        case '}': token.type = TOKEN_RBRACE; token.length = 1; return token;
        case '[': token.type = TOKEN_LBRACKET; token.length = 1; return token;
        case ']': token.type = TOKEN_RBRACKET; token.length = 1; return token;
        case ':': token.type = TOKEN_COLON; token.length = 1; return token;
        case ',': token.type = TOKEN_COMMA; token.length = 1; return token;
        case '"':
            while (*scanner.current != '"' && *scanner.current != '\0') {
                if (*scanner.current == '\\' && *(scanner.current + 1) == '"') scanner.current += 2;
                else scanner.current++;
            }
            if (*scanner.current == '\0') token.type = TOKEN_ERROR;
            else { scanner.current++; token.type = TOKEN_STRING; }
            token.length = (int)(scanner.current - token.start);
            return token;
    }
    token.type = TOKEN_ERROR; return token;
}

/* --- Parser Implementation --- */

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = next_token();
        if (parser.current.type != TOKEN_ERROR) break;
        parser.had_error = true;
    }
}

static bool check(nyxj_token_type type) { return parser.current.type == type; }
static bool match(nyxj_token_type type) { if (!check(type)) return false; advance(); return true; }

static nyxj_value* create_value(nyxj_type type) {
    nyxj_value* v = malloc(sizeof(nyxj_value));
    if (v) v->type = type;
    return v;
}

static nyxj_value* parse_value() {
    if (match(TOKEN_STRING)) {
        nyxj_value* v = create_value(NYXJ_STRING);
        int len = parser.previous.length - 2;
        v->as.string = malloc(len + 1);
        memcpy(v->as.string, parser.previous.start + 1, len);
        v->as.string[len] = '\0';
        return v;
    }
    if (match(TOKEN_NUMBER)) {
        nyxj_value* v = create_value(NYXJ_NUMBER);
        v->as.number = strtod(parser.previous.start, NULL);
        return v;
    }
    if (match(TOKEN_TRUE)) { nyxj_value* v = create_value(NYXJ_BOOL); v->as.boolean = true; return v; }
    if (match(TOKEN_FALSE)) { nyxj_value* v = create_value(NYXJ_BOOL); v->as.boolean = false; return v; }
    if (match(TOKEN_NULL)) return create_value(NYXJ_NULL);
    if (match(TOKEN_LBRACE)) return parse_object();
    if (match(TOKEN_LBRACKET)) return parse_array();
    parser.had_error = true; return NULL;
}

static nyxj_value* parse_object() {
    nyxj_value* obj = create_value(NYXJ_OBJECT);
    obj->as.object.pairs = NULL; obj->as.object.count = 0;
    nyxj_pair* last = NULL;
    if (!check(TOKEN_RBRACE)) {
        do {
            if (!match(TOKEN_STRING)) { parser.had_error = true; break; }
            nyxj_pair* pair = malloc(sizeof(nyxj_pair));
            int klen = parser.previous.length - 2;
            pair->key = malloc(klen + 1); memcpy(pair->key, parser.previous.start + 1, klen); pair->key[klen] = '\0';
            if (!match(TOKEN_COLON)) { parser.had_error = true; break; }
            pair->value = parse_value(); pair->next = NULL;
            if (!last) obj->as.object.pairs = pair; else last->next = pair;
            last = pair; obj->as.object.count++;
        } while (match(TOKEN_COMMA));
    }
    if (!match(TOKEN_RBRACE)) parser.had_error = true;
    return obj;
}

static nyxj_value* parse_array() {
    nyxj_value* arr = create_value(NYXJ_ARRAY);
    arr->as.array.count = 0; arr->as.array.items = NULL;
    if (!check(TOKEN_RBRACKET)) {
        do {
            nyxj_value* item = parse_value();
            arr->as.array.count++;
            arr->as.array.items = realloc(arr->as.array.items, sizeof(nyxj_value*) * arr->as.array.count);
            arr->as.array.items[arr->as.array.count - 1] = item;
        } while (match(TOKEN_COMMA));
    }
    if (!match(TOKEN_RBRACKET)) parser.had_error = true;
    return arr;
}

/* --- Serialization Implementation --- */

static char* append_str(char* dest, size_t* capacity, const char* src) {
    size_t slen = strlen(src);
    size_t dlen = dest ? strlen(dest) : 0;
    if (dlen + slen + 1 > *capacity) {
        *capacity = (dlen + slen + 1) * 2;
        dest = realloc(dest, *capacity);
    }
    if (dlen == 0) dest[0] = '\0';
    strcat(dest, src);
    return dest;
}

static void serialize_recursive(nyxj_value* v, char** buffer, size_t* capacity) {
    if (!v) return;
    char temp[64];
    switch (v->type) {
        case NYXJ_NULL: *buffer = append_str(*buffer, capacity, "null"); break;
        case NYXJ_BOOL: *buffer = append_str(*buffer, capacity, v->as.boolean ? "true" : "false"); break;
        case NYXJ_NUMBER: sprintf(temp, "%g", v->as.number); *buffer = append_str(*buffer, capacity, temp); break;
        case NYXJ_STRING:
            *buffer = append_str(*buffer, capacity, "\"");
            *buffer = append_str(*buffer, capacity, v->as.string);
            *buffer = append_str(*buffer, capacity, "\"");
            break;
        case NYXJ_ARRAY:
            *buffer = append_str(*buffer, capacity, "[");
            for (int i = 0; i < v->as.array.count; i++) {
                serialize_recursive(v->as.array.items[i], buffer, capacity);
                if (i < v->as.array.count - 1) *buffer = append_str(*buffer, capacity, ",");
            }
            *buffer = append_str(*buffer, capacity, "]");
            break;
        case NYXJ_OBJECT:
            *buffer = append_str(*buffer, capacity, "{");
            nyxj_pair* p = v->as.object.pairs;
            while (p) {
                *buffer = append_str(*buffer, capacity, "\"");
                *buffer = append_str(*buffer, capacity, p->key);
                *buffer = append_str(*buffer, capacity, "\":");
                serialize_recursive(p->value, buffer, capacity);
                p = p->next;
                if (p) *buffer = append_str(*buffer, capacity, ",");
            }
            *buffer = append_str(*buffer, capacity, "}");
            break;
    }
}

char* nyxj_serialize(nyxj_value* v) {
    size_t cap = 128;
    char* buf = malloc(cap); buf[0] = '\0';
    serialize_recursive(v, &buf, &cap);
    return buf;
}

/* --- Cleanup & Public API --- */

void nyxj_free_value(nyxj_value* v) {
    if (!v) return;
    if (v->type == NYXJ_STRING) free(v->as.string);
    else if (v->type == NYXJ_ARRAY) {
        for (int i = 0; i < v->as.array.count; i++) nyxj_free_value(v->as.array.items[i]);
        free(v->as.array.items);
    } else if (v->type == NYXJ_OBJECT) {
        nyxj_pair* p = v->as.object.pairs;
        while (p) { nyxj_pair* n = p->next; free(p->key); nyxj_free_value(p->value); free(p); p = n; }
    }
    free(v);
}

nyxj_result nyxj_parse(const char* json_str) {
    scanner.start = json_str; scanner.current = json_str;
    parser.had_error = false; advance();
    nyxj_value* root = parse_value();
    if (!check(TOKEN_EOF)) parser.had_error = true;
    nyxj_result r = { !parser.had_error, parser.had_error ? "Syntax Error" : NULL, root };
    return r;
}