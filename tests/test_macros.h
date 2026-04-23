#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include <stdio.h>

// A simple macro to check a condition
#define ASSERT_TEST(cond, message) \
    do { \
        if (!(cond)) { \
            printf("[FAIL] %s\n", message); \
            return false; \
        } else { \
            printf("[PASS] %s\n", message); \
        } \
    } while (0)

#endif