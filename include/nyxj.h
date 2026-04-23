#ifndef NYXJ_H
#define NYXJ_H

#include <stdbool.h>

typedef enum {
    NYXJ_NULL,
    NYXJ_BOOL,
    NYXJ_NUMBER,
    NYXJ_STRING,
    NYXJ_ARRAY,
    NYXJ_OBJECT
} nyxj_type;

typedef struct nyxj_value nyxj_value;

typedef struct nyxj_pair {
    char* key;
    nyxj_value* value;
    struct nyxj_pair* next;
} nyxj_pair;

struct nyxj_value {
    nyxj_type type;
    union {
        bool boolean;
        double number;
        char* string;
        struct {
            nyxj_value** items;
            int count;
        } array;
        struct {
            nyxj_pair* pairs;
            int count;
        } object;
    } as;
};

typedef struct {
    bool is_valid;
    const char* error_msg;
    nyxj_value* root;
} nyxj_result;

nyxj_result nyxj_parse(const char* json_str);

#endif