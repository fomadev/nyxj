#ifdef _WIN32
  #define _CRT_SECURE_NO_WARNINGS
#else
  #define _GNU_SOURCE
#endif

#include "../../vendor/mongoose/mongoose.h"
#include "../../include/nyxj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void fn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_CONNECT) {
    // 1. Message created with NyxJ
    nyxj_value* root = malloc(sizeof(nyxj_value));
    root->type = NYXJ_OBJECT;
    
    nyxj_pair* p = malloc(sizeof(nyxj_pair));
    p->key = strdup("texte");
    
    nyxj_value* val = malloc(sizeof(nyxj_value));
    val->type = NYXJ_STRING;
    val->as.string = strdup("Hello from NyxJ v1.0.0!");
    
    p->value = val;
    p->next = NULL;
    root->as.object.pairs = p;
    root->as.object.count = 1;

    char* json_payload = nyxj_serialize(root);

    // 2. Sending the HTTP POST request to FastAPI
    mg_printf(c, "POST /envoyer HTTP/1.1\r\n"
                 "Host: localhost:8000\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n\r\n"
                 "%s", (int)strlen(json_payload), json_payload);

    free(json_payload);
    nyxj_free_value(root);
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    printf("Python server response : %.*s\n", (int) hm->message.len, hm->message.buf);
    c->is_closing = 1; // We close after the answer
  } else if (ev == MG_EV_ERROR) {
    printf("Connection error !\n");
  }
}

int main() {
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  mg_http_connect(&mgr, "http://localhost:8000", fn, NULL);
  for (;;) mg_mgr_poll(&mgr, 8080);
  mg_mgr_free(&mgr);
  return 0;
}