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