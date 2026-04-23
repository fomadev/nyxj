#include "../../include/nyxj.h"
#include <stdio.h>

int main() {
    const char* json = "{\"name\": \"NyxJ Engine\", \"version\": 1.0.0, \"is_stable\": true}";
    
    printf("--- NyxJ CLI Debugger ---\n");
    printf("Parsing: %s\n", json);
    
    nyxj_result res = nyxj_parse(json);

    if (res.is_valid) {
        printf("Status: Success! JSON tree built in memory.\n");
        
        // Using the Getter API
        const char* name = nyxj_get_string(res.root, "name");
        double version   = nyxj_get_number(res.root, "version");
        bool stable      = nyxj_get_bool(res.root, "is_stable");

        printf("Data Extracted:\n");
        printf(" - Project: %s\n", name ? name : "N/A");
        printf(" - Version: %.1f\n", version);
        printf(" - Stable: %s\n", stable ? "Yes" : "No");

        // Essential: Freeing memory
        nyxj_free_value(res.root);
        printf("\nMemory cleaned up successfully.\n");
    } else {
        printf("Status: Error! %s\n", res.error_msg);
    }

    return 0;
}