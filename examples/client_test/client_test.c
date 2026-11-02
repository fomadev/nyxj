/*
 * HTTP client example: builds a JSON message with the builder API and posts
 * it to the FastAPI service in main.py (uvicorn main:app --port 8000).
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_http.h"

#include <stdio.h>

int main(int argc, char** argv) {
    const char* base = argc > 1 ? argv[1] : "http://localhost:8000";
    char url[256];

    /* 1. Message created with the NyxJ builder API */
    nyxj_value* msg = nyxj_new_object();
    nyxj_object_set_string(msg, "texte", "Hello from NyxJ v" NYXJ_VERSION "!");

    /* 2. POST it with the native HTTP client */
    snprintf(url, sizeof(url), "%s/envoyer", base);
    nyxj_http_response resp;
    nyxj_error err;
    if (!nyxj_http_post_json(url, msg, 5000, &resp, &err)) {
        fprintf(stderr, "Connection error: %s\n", err.message);
        nyxj_free_value(msg);
        return 1;
    }
    printf("Python server response (%d): %s\n", resp.status, resp.body);
    nyxj_http_response_free(&resp);

    /* 3. Read the list back and walk it */
    snprintf(url, sizeof(url), "%s/messages", base);
    if (nyxj_http_get(url, 5000, &resp, &err)) {
        nyxj_value* data = nyxj_http_response_json(&resp, &err);
        nyxj_value* list = nyxj_get_member(data, "messages");
        for (int i = 0; i < nyxj_size(list); i++) printf("  - %s\n", nyxj_array_get(list, i)->as.string);
        nyxj_free_value(data);
        nyxj_http_response_free(&resp);
    }
    nyxj_free_value(msg);
    return 0;
}
