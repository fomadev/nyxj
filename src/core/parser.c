#include "../../include/nyxj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Internal Structures --- */

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

typedef struct {
    const char* start;
    const char* current;
} Scanner;

typedef struct {
    nyxj_token current;
    nyxj_token previous;
    bool had_error;
} Parser;

/* --- Global States --- */

Scanner scanner;
Parser parser;

/* --- Forward Declarations --- */

static nyxj_value* parse_value();
static nyxj_value* parse_object();
static nyxj_value* parse_array();

/* --- Scanner & Lexer Logic --- */

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alpha(char c) { 
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; 
}

static void skip_whitespace() {
    for (;;) {
        char c = *scanner.current;
        switch (c) {
            case ' ': case '\r': case '\t': case '\n':
                scanner.current++;
                break;
            default: return;
        }
    }
}

static nyxj_token_type check_keyword(const char* start, int length, const char* rest, int rest_len, nyxj_token_type type) {
    if ((int)(scanner.current - start) == length + rest_len && 
        memcmp(start + length, rest, rest_len) == 0) {
        return type;
    }
    return TOKEN_ERROR;
}

static nyxj_token_type identifier_type(const char* start) {
    switch (start[0]) {
        case 't': return check_keyword(start, 1, "rue", 3, TOKEN_TRUE);
        case 'f': return check_keyword(start, 1, "alse", 4, TOKEN_FALSE);
        case 'n': return check_keyword(start, 1, "ull", 3, TOKEN_NULL);
    }
    return TOKEN_ERROR;
}

nyxj_token next_token() {
    skip_whitespace();
    nyxj_token token;
    token.start = scanner.current;
    
    if (*scanner.current == '\0') {
        token.type = TOKEN_EOF;
        token.length = 0;
        return token;
    }

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
    token.type = TOKEN_ERROR;
    token.length = 1;
    return token;
}

/* --- Parser Infrastructure --- */

static void advance() {
    parser.previous = parser.current;
    for (;;) {
        parser.current = next_token();
        if (parser.current.type != TOKEN_ERROR) break;
        parser.had_error = true;
    }
}

static bool check(nyxj_token_type type) { return parser.current.type == type; }

static bool match(nyxj_token_type type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/* --- Recursive Descent with Data Storage --- */

static nyxj_value* create_value(nyxj_type type) {
    nyxj_value* v = malloc(sizeof(nyxj_value));
    if (v) v->type = type;
    return v;
}

static nyxj_value* parse_value() {
    if (match(TOKEN_STRING)) {
        nyxj_value* v = create_value(NYXJ_STRING);
        // Copying string without quotes
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
    if (match(TOKEN_TRUE)) {
        nyxj_value* v = create_value(NYXJ_BOOL);
        v->as.boolean = true;
        return v;
    }
    if (match(TOKEN_FALSE)) {
        nyxj_value* v = create_value(NYXJ_BOOL);
        v->as.boolean = false;
        return v;
    }
    if (match(TOKEN_NULL)) {
        return create_value(NYXJ_NULL);
    }
    if (match(TOKEN_LBRACE)) return parse_object();
    if (match(TOKEN_LBRACKET)) return parse_array();

    parser.had_error = true;
    return NULL;
}

static nyxj_value* parse_object() {
    nyxj_value* obj_val = create_value(NYXJ_OBJECT);
    obj_val->as.object.pairs = NULL;
    obj_val->as.object.count = 0;
    nyxj_pair* last_pair = NULL;

    if (!check(TOKEN_RBRACE)) {
        do {
            if (!match(TOKEN_STRING)) { parser.had_error = true; return obj_val; }
            
            nyxj_pair* pair = malloc(sizeof(nyxj_pair));
            int key_len = parser.previous.length - 2;
            pair->key = malloc(key_len + 1);
            memcpy(pair->key, parser.previous.start + 1, key_len);
            pair->key[key_len] = '\0';
            pair->next = NULL;

            if (!match(TOKEN_COLON)) { parser.had_error = true; return obj_val; }

            pair->value = parse_value();

            if (last_pair == NULL) obj_val->as.object.pairs = pair;
            else last_pair->next = pair;
            
            last_pair = pair;
            obj_val->as.object.count++;

        } while (match(TOKEN_COMMA));
    }

    if (!match(TOKEN_RBRACE)) parser.had_error = true;
    return obj_val;
}

static nyxj_value* parse_array() {
    nyxj_value* arr_val = create_value(NYXJ_ARRAY);
    arr_val->as.array.count = 0;
    arr_val->as.array.items = NULL;

    // Simplified dynamic array growth
    if (!check(TOKEN_RBRACKET)) {
        do {
            nyxj_value* item = parse_value();
            arr_val->as.array.count++;
            arr_val->as.array.items = realloc(arr_val->as.array.items, sizeof(nyxj_value*) * arr_val->as.array.count);
            arr_val->as.array.items[arr_val->as.array.count - 1] = item;
        } while (match(TOKEN_COMMA));
    }

    if (!match(TOKEN_RBRACKET)) parser.had_error = true;
    return arr_val;
}

/* --- Public API --- */

nyxj_result nyxj_parse(const char* json_str) {
    init_scanner(json_str);
    parser.had_error = false;
    advance();

    nyxj_value* root = parse_value();

    if (!check(TOKEN_EOF)) parser.had_error = true;

    nyxj_result result;
    result.is_valid = !parser.had_error;
    result.error_msg = parser.had_error ? "Invalid JSON syntax" : NULL;
    result.root = root;
    
    return result;
}