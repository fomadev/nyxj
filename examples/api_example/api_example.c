/*
 * API example: a tiny service consumed by index.html / script.js.
 * Build with `make examples`, run build/examples/api_example, then open
 * examples/api_example/index.html in a browser.
 */
#include "../../include/nyxj.h"
#include "../../include/nyxj_http.h"

#include <stdio.h>
#include <string.h>

/* POST /api/greet {"name": "..."} -> {"message": "...", "status": "success"} */
static void greet(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = nyxj_req_json(req);
    if (!body) {
        nyxj_res_error(res, 400, "invalid_json", nyxj_req_json_error(req)->message);
        return;
    }
    const char* name = nyxj_get_string(body, "name");
    char message[256];
    snprintf(message, sizeof(message), "Hello %s, NyxJ v%s was waiting for you!", name ? name : "Unknown", NYXJ_VERSION);

    nyxj_value* out = nyxj_new_object();
    nyxj_object_set_string(out, "message", message);
    nyxj_object_set_string(out, "status", "success");
    nyxj_res_json(res, 200, out);
}

/* GET /api/user/:id shows route parameters. */
static void get_user(nyxj_request* req, nyxj_response* res) {
    nyxj_value* out = nyxj_new_object();
    nyxj_object_set_string(out, "id", nyxj_req_param(req, "id"));
    nyxj_object_set_string(out, "role", "explorer");
    nyxj_res_json(res, 200, out);
}

int main(void) {
    nyxj_app_config cfg = nyxj_app_config_default();
    cfg.listen_url = "http://0.0.0.0:8081";
    cfg.title = "NyxJ greeting API";
    cfg.cors = true; /* The page is opened from file:// */

    nyxj_app* app = nyxj_app_create(&cfg);
    nyxj_route_doc doc = {"Greets the caller", NULL, "demo", "{\"type\":\"object\",\"required\":[\"name\"]}", NULL};
    nyxj_app_route_ex(app, "POST", "/api/greet", greet, NULL, NYXJ_ROUTE_ASYNC, &doc);
    nyxj_app_get(app, "/api/user/:id", get_user, NULL);

    printf("NyxJ API server for JS started on http://localhost:8081 (docs: /docs)\n");
    int rc = nyxj_app_run(app);
    nyxj_app_destroy(app);
    return rc;
}
