#include <stdbool.h>
#include <string.h>

/* --- Définitions des types --- */

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
    TOKEN_EOF,       // Fin de chaîne
    TOKEN_ERROR      // Erreur de syntaxe
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

Scanner scanner;

/* --- Fonctions Utilitaires --- */

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

// Vérifie si le mot-clé correspond exactement
static nyxj_token_type check_keyword(const char* start, int length, const char* rest, int rest_len, nyxj_token_type type) {
    if ((int)(scanner.current - start) == length + rest_len && 
        memcmp(start + length, rest, rest_len) == 0) {
        return type;
    }
    return TOKEN_ERROR;
}

// Identifie true, false, ou null
static nyxj_token_type identifier_type() {
    int len = (int)(scanner.current - (scanner.current - 1)); // Longueur simplifiée pour le switch
    switch (scanner.current[-1]) {
        case 't': return check_keyword(scanner.current - 1, 1, "rue", 3, TOKEN_TRUE);
        case 'f': return check_keyword(scanner.current - 1, 1, "alse", 4, TOKEN_FALSE);
        case 'n': return check_keyword(scanner.current - 1, 1, "ull", 3, TOKEN_NULL);
    }
    return TOKEN_ERROR;
}

/* --- Logique Principale --- */

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

    // 1. Mots-clés (true, false, null)
    if (is_alpha(c)) {
        while (is_alpha(*scanner.current)) scanner.current++;
        token.type = identifier_type();
        token.length = (int)(scanner.current - token.start);
        return token;
    }

    // 2. Nombres
    if (is_digit(c) || c == '-') {
        while (is_digit(*scanner.current)) scanner.current++;

        // Partie fractionnaire
        if (*scanner.current == '.' && is_digit(scanner.current[1])) {
            scanner.current++; // Consomme le '.'
            while (is_digit(*scanner.current)) scanner.current++;
        }
        
        token.type = TOKEN_NUMBER;
        token.length = (int)(scanner.current - token.start);
        return token;
    }

    // 3. Symboles uniques
    switch (c) {
        case '{': token.type = TOKEN_LBRACE; token.length = 1; return token;
        case '}': token.type = TOKEN_RBRACE; token.length = 1; return token;
        case '[': token.type = TOKEN_LBRACKET; token.length = 1; return token;
        case ']': token.type = TOKEN_RBRACKET; token.length = 1; return token;
        case ':': token.type = TOKEN_COLON; token.length = 1; return token;
        case ',': token.type = TOKEN_COMMA; token.length = 1; return token;
        
        case '"': // Chaînes de caractères
            while (*scanner.current != '"' && *scanner.current != '\0') {
                // Gestion basique du caractère d'échappement \"
                if (*scanner.current == '\\' && *(scanner.current + 1) == '"') {
                    scanner.current += 2;
                } else {
                    scanner.current++;
                }
            }
            if (*scanner.current == '\0') {
                token.type = TOKEN_ERROR;
            } else {
                scanner.current++; // Fermer le guillemet
                token.type = TOKEN_STRING;
            }
            token.length = (int)(scanner.current - token.start);
            return token;
    }

    token.type = TOKEN_ERROR;
    token.length = 1;
    return token;
}