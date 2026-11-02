/*
 * Example plugin. Build: `make plugin`, load it with
 *   nyxj-server --plugin build/plugins/hello_plugin.so   (.dll on Windows)
 * It adds GET /hello/:name and POST /hello/echo without recompiling NyxJ.
 */
#include "nyxj_plugin.h"

#include <stdio.h>

static const nyxj_host_api* api = NULL;

static void hello(nyxj_request* req, nyxj_response* res) {
    char greeting[160];
    snprintf(greeting, sizeof(greeting), "Hello %s, from a dynamically loaded plugin!", api->req_param(req, "name"));
    nyxj_value* o = api->new_object();
    api->object_set(o, "message", api->new_string(greeting));
    api->object_set(o, "plugin", api->new_string("hello"));
    api->res_json(res, 200, o);
}

static void echo(nyxj_request* req, nyxj_response* res) {
    nyxj_value* body = api->req_json(req);
    if (!body) {
        api->res_error(res, 400, "invalid_json", "expected a JSON body");
        return;
    }
    nyxj_value* o = api->new_object();
    api->object_set(o, "echo", api->parse(req->body, req->body_len, NULL).root);
    api->res_json(res, 200, o);
}

NYXJ_PLUGIN_EXPORT const nyxj_plugin_meta* nyxj_plugin_describe(void) {
    static const nyxj_plugin_meta meta = {"hello", "1.0.0", "Greeting routes demo plugin", NYXJ_PLUGIN_API_VERSION};
    return &meta;
}

NYXJ_PLUGIN_EXPORT int nyxj_plugin_init(nyxj_app* app, const nyxj_host_api* host) {
    api = host;
    nyxj_route_doc doc = {"Greeting served by the hello plugin", NULL, "plugins", NULL, NULL};
    if (!api->route(app, "GET", "/hello/:name", hello, NULL, 0, &doc)) return 1;
    doc.summary = "Echo a JSON body (hello plugin)";
    if (!api->route(app, "POST", "/hello/echo", echo, NULL, 0, &doc)) return 1;
    api->log(NYXJ_LOG_INFO, "hello-plugin", __FILE__, __LINE__, "routes registered");
    return 0;
}

NYXJ_PLUGIN_EXPORT void nyxj_plugin_shutdown(void) { api = NULL; }
