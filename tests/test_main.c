#include "../include/nyxj.h"
#include "../include/nyxj_log.h"
#include "test.h"

#include <stdlib.h>

int g_checks = 0;
int g_failures = 0;

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : NULL;
    nyxj_log_set_level(NYXJ_LOG_WARN);
    printf("--- NyxJ %s test suite (SIMD: %s) ---\n", NYXJ_VERSION, nyxj_simd_backend());
    if (!only || !strcmp(only, "core")) { printf("core\n"); suite_core(); }
    if (!only || !strcmp(only, "modules")) { printf("modules\n"); suite_modules(); }
    if (!only || !strcmp(only, "runtime")) { printf("runtime\n"); suite_runtime(); }
    if (!only || !strcmp(only, "http")) { printf("http\n"); suite_http(); }
    printf("--- %d checks, %d failure(s) ---\n", g_checks, g_failures);
    if (g_failures == 0) printf("All tests passed!\n");
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
