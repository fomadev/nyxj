#include "../vendor/mongoose/mongoose.h"
#include "../include/nyxj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mongoose event handler (3 arguments seulement en v7+)
static void fn(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;

        // Utilisation de mg_match pour vérifier l'URI
        if (mg_match(hm->uri, mg_str("/api/data"), NULL)) {
            
            // Extraction du body (on utilise .buf et non .ptr)
            char* body = malloc(hm->body.len + 1);
            memcpy(body, hm->body.buf, hm->body.len);
            body[hm->body.len] = '\0';

            nyxj_result res = nyxj_parse(body);
            free(body);

            if (res.is_valid) {
                char* response_json = nyxj_serialize(res.root);
                mg_http_reply(c, 200, "Content-Type: application/json\r\n", "%s", response_json);
                
                free(response_json);
                nyxj_free_value(res.root);
            } else {
                mg_http_reply(c, 400, "Content-Type: application/json\r\n", "{\"error\": \"Invalid JSON\"}");
            }
        } else {
            mg_http_reply(c, 404, "", "Not Found");
        }
    }
}

int main() {
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    printf("NyxJ Server started on http://localhost:8000\n");
    mg_http_listen(&mgr, "http://0.0.0.0:8000", fn, NULL);
    for (;;) mg_mgr_poll(&mgr, 1000);
    mg_mgr_free(&mgr);
    return 0;
}