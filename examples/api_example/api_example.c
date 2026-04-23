#ifdef _WIN32
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define _GNU_SOURCE
#endif

#include "../../vendor/mongoose/mongoose.h"
#include "../../include/nyxj.h"
#include <stdio.h>
#include <string.h>

static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        // CORS handling (so that JS can call C without blocking)
        if (mg_match(hm->method, mg_str("OPTIONS"), NULL)) {
            mg_http_reply(c, 204, "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: Content-Type\r\n", "");
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/greet"), NULL)) {
            // 1. Parsing the JSON received from the JS
            char* body = malloc(hm->body.len + 1);
            memcpy(body, hm->body.buf, hm->body.len);
            body[hm->body.len] = '\0';

            nyxj_result res = nyxj_parse(body);
            free(body);

            if (res.is_valid) {
                const char* name = nyxj_get_string(res.root, "name");
                
                // 2. Creating the response with NyxJ
                // We simulate a response structure
                char response_buf[256];
                snprintf(response_buf, sizeof(response_buf), 
                         "{\"message\": \"Hello %s, NyxJ v1.0.0 was waiting for you!\", \"status\": \"success\"}", 
                         name ? name : "Unknown");

                mg_http_reply(c, 200, "Content-Type: application/json\r\n"
                                     "Access-Control-Allow-Origin: *\r\n", 
                              "%s", response_buf);
                
                nyxj_free_value(res.root);
            } else {
                mg_http_reply(c, 400, "Access-Control-Allow-Origin: *\r\n", "{\"error\": \"Invalid JSON\"}");
            }
        }
    }
}

int main() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    // Change 8080 to 8081 here
    printf("NyxJ API server for JS started on http://localhost:8081\n");
    mg_http_listen(&mgr, "http://0.0.0.0:8081", fn, NULL); 
    for (;;) mg_mgr_poll(&mgr, 1000);
    return 0;
}