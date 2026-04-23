#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* --- Types Definitions --- */

typedef enum {
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_STRING,    // "..."
    TOKEN_NUMBER,    // 123.45
    TOKEN_COLON,     // :
    TOKEN_COMMA,     // ,
    TOKEN_TRUE,      // true
    TOKEN_FALSE,     // false
    TOKEN_NULL,      // null
    TOKEN_EOF,       // End of string
    TOKEN_ERROR      // Syntax error
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

typedef struct {
    bool is_valid;
    const char* error_msg;
} nyxj_result;

/* --- Global States --- */

Scanner scanner;
Parser parser;

/* --- Lexer Utilities (Scanner) --- */

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static void skip_whitespace() {
    for (;;) {
        char c = *scanner.current;
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                scanner.current++;
                break;
            default:
                return;
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

static nyxj_token_type identifier_type() {
    switch (scanner.current[-(int)(scanner.current - (scanner.current - (int)(scanner.current - (scanner.current-1))))]) { 
        // Note: simplified logic to look back from the current position
    }
    
    // Logic for true, false, null
    const char* s = scanner.current;
    int len = (int)(scanner.current - (scanner.current - 1)); 
    
    // We analyze the first character of the word
    char first = *(scanner.current - (int)(scanner.current - (scanner.current - (int)(scanner.current - (scanner.current - 1)))) );
    // Manual check to stay within NyxJ performance goals
    const char* start = scanner.current;
    while(is_alpha(*(start-1))) start--; 

    switch (*start) {
        case 't': return check_keyword(start, 1, "rue", 3, TOKEN_TRUE);
        case 'f': return check_keyword(start, 1, "alse", 4, TOKEN_FALSE);
        case 'n': return check_keyword(start, 1, "ull", 3, TOKEN_NULL);
    }
    return TOKEN_ERROR;
}

/* --- Lexer Core (Next Token) --- */

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
        token.type = identifier_type();
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
                if (*scanner.current == '\\' && *(scanner.current + 1) == '"') {
                    scanner.current += 2;
                } else {
                    scanner.current++;
                }
            }
            if (*scanner.current == '\0') {
                token.type = TOKEN_ERROR;
            } else {
                scanner.current++;
                token.type = TOKEN_STRING;
            }
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

        // Log error or recovery here if needed
        parser.had_error = true;
    }
}

static bool check(nyxj_token_type type) {
    return parser.current.type == type;
}

static bool match(nyxj_token_type type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/* --- Recursive Descent Logic (Entry Points) --- */

nyxj_result nyxj_parse(const char* json_str) {
    init_scanner(json_str);
    parser.had_error = false;
    
    advance(); // Load the first token
    
    // Basic structural check for JSON
    if (match(TOKEN_LBRACE)) {
        // TODO: Logic for parsing object: parse_object()
        printf("Info: JSON Object detected\n");
    } else if (match(TOKEN_LBRACKET)) {
        // TODO: Logic for parsing array: parse_array()
        printf("Info: JSON Array detected\n");
    } else {
        parser.had_error = true;
    }

    nyxj_result result;
    result.is_valid = !parser.had_error;
    result.error_msg = parser.had_error ? "Invalid JSON format" : NULL;
    
    return result;
}