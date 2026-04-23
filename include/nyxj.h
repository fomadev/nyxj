#ifndef NYXJ_H
#define NYXJ_H

#include <stdio.h>
#include <stdbool.h>

// Types de base pour NyxJ
typedef enum {
    NYXJ_NULL,
    NYXJ_BOOL,
    NYXJ_NUMBER,
    NYXJ_STRING,
    NYXJ_ARRAY,
    NYXJ_OBJECT
} nyxj_type;

// Structure de résultat
typedef struct {
    bool is_valid;
    const char* error_msg;
} nyxj_result;

// Prototypes
nyxj_result nyxj_parse(const char* json_str);

#endif // NYXJ_H