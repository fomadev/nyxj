#ifdef _WIN32
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define _GNU_SOURCE
#endif

#include "../vendor/mongoose/mongoose.h"
#include "../include/nyxj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * NyxJ API Event Handler
 * Processes incoming HTTP messages, parses JSON, and returns responses.
 */
static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        // Route: POST /api/data
        if (mg_match(hm->uri, mg_str("/api/data"), NULL)) {
            
            // 1. Extract body safely (using .buf for Mongoose v7+)
            char* body = malloc(hm->body.len + 1);
            if (body == NULL) {
                mg_http_reply(c, 500, NULL, "{\"error\": \"Internal Server Error\"}");
                return;
            }
            memcpy(body, hm->body.buf, hm->body.len);
            body[hm->body.len] = '\0';

            // 2. Parse incoming JSON with NyxJ Engine
            nyxj_result res = nyxj_parse(body);
            free(body);

            if (res.is_valid) {
                // 3. Serialize the tree back to JSON string
                char* response_json = nyxj_serialize(res.root);
                
                // 4. Send Success Response
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response_json);
                
                // Cleanup
                free(response_json);
                nyxj_free_value(res.root);
            } else {
                // 4. Send Error Response
                mg_http_reply(c, 400, "Content-Type: application/json\r\n", 
                              "{\"error\": \"Invalid JSON syntax\", \"details\": \"%s\"}", 
                              res.error_msg);
            }
        } else {
            // Default 404 handler
            mg_http_reply(c, 404, "Content-Type: application/json\r\n", "{\"error\": \"Not Found\"}");
        }
    }
}

int main(void) {
    struct mg_mgr mgr;
    const char *url = "http://0.0.0.0:8000";

    mg_mgr_init(&mgr);
    
    if (mg_http_listen(&mgr, url, fn, NULL) == NULL) {
        fprintf(stderr, "Error: Failed to start listener on %s\n", url);
        return 1;
    }

    printf("NyxJ Micro-service v1.0.0\n");
    printf("Server active at: %s\n", url);
    printf("Press Ctrl+C to stop.\n");

    // Infinite event loop (poll every 1000ms)
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }

    mg_mgr_free(&mgr);
    return 0;
}