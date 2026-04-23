#include "nyxj.h"
#include "test_macros.h"

bool test_empty_object() {
    nyxj_result res = nyxj_parse("{}");
    ASSERT_TEST(res.is_valid == true, "The empty object must be valid");
    return true;
}

int main() {
    printf("--- NyxJ testing launch ---\n");
    if (test_empty_object()) {
        printf("All tests passed!\n");
    }
    return 0;
}