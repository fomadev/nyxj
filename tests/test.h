/* Minimal test framework: CHECK records failures and keeps going. */
#ifndef NYXJ_TEST_H
#define NYXJ_TEST_H

#include <stdio.h>
#include <string.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond)                                                              \
    do {                                                                         \
        g_checks++;                                                              \
        if (!(cond)) {                                                           \
            g_failures++;                                                        \
            printf("    [FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                        \
    } while (0)

#define CHECK_STR(a, b) CHECK((a) != NULL && (b) != NULL && strcmp((a), (b)) == 0)

#define RUN(fn)                                       \
    do {                                              \
        int before = g_failures;                      \
        fn();                                         \
        printf("  %s %s\n", g_failures == before ? "[PASS]" : "[FAIL]", #fn); \
    } while (0)

void suite_core(void);
void suite_modules(void);
void suite_runtime(void);
void suite_http(void);

#endif
