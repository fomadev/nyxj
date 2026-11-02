#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "app_internal.h"
#include "../../include/nyxj_cluster.h"
#include "../../include/nyxj_log.h"
#include "../core/internal.h"

#include <stdio.h>
#include <string.h>

#define MOD "cluster"

enum { ALIVE = 0, SUSPECT = 1, DEAD = 2 };
static const char* status_names[] = {"alive", "suspect", "dead"};

typedef struct {
    char id[17];
    char name[64];
    char gossip[96];
    char http[128];
    uint64_t heartbeat;
    double load;
    int status;
    uint64_t last_seen_ms;
} member;

struct nyxj_cluster {
    nyxj_app* app;
    member self;
    member* members;
    int count, cap;
    char** seeds;
    int nseeds;
    int interval_ms, suspect_ms, dead_ms;
    struct mg_connection* udp;
    bool stopped;
    nyxj_mutex lock;
};

static member* find_member(nyxj_cluster* c, const char* id) {
    for (int i = 0; i < c->count; i++)
        if (strcmp(c->members[i].id, id) == 0) return &c->members[i];
    return NULL;
}

static void copy_field(char* dst, size_t n, const nyxj_value* obj, const char* key) {
    const char* s = nyxj_get_string(obj, key);
    if (s) snprintf(dst, n, "%s", s);
}

static void merge(nyxj_cluster* c, const nyxj_value* msg) {
    const nyxj_value* list = nyxj_get_member(msg, "members");
    if (!list || list->type != NYXJ_ARRAY) return;
    uint64_t now = nyxj_now_ms();
    nyxj_mutex_lock(&c->lock);
    for (int i = 0; i < list->as.array.count; i++) {
        const nyxj_value* m = list->as.array.items[i];
        const char* id = nyxj_get_string(m, "id");
        if (!id || strlen(id) >= sizeof(c->self.id) || strcmp(id, c->self.id) == 0) continue;
        uint64_t hb = (uint64_t)nyxj_get_number(m, "hb");
        member* e = find_member(c, id);
        if (!e) {
            if (c->count == c->cap) {
                int ncap = c->cap ? c->cap * 2 : 8;
                member* g = realloc(c->members, sizeof(member) * (size_t)ncap);
                if (!g) break;
                c->members = g;
                c->cap = ncap;
            }
            e = &c->members[c->count++];
            memset(e, 0, sizeof(*e));
            snprintf(e->id, sizeof(e->id), "%s", id);
            NYXJ_INFO(MOD, "discovered peer %s", id);
        } else if (hb <= e->heartbeat) {
            continue; /* Stale gossip */
        }
        e->heartbeat = hb;
        e->load = nyxj_get_number(m, "load");
        copy_field(e->name, sizeof(e->name), m, "name");
        copy_field(e->gossip, sizeof(e->gossip), m, "gossip");
        copy_field(e->http, sizeof(e->http), m, "http");
        if (e->status != ALIVE) NYXJ_INFO(MOD, "peer %s is alive again", e->id);
        e->status = ALIVE;
        e->last_seen_ms = now;
    }
    nyxj_mutex_unlock(&c->lock);
}

static void udp_in(struct mg_connection* conn, int ev, void* ev_data) {
    (void)ev_data;
    if (ev != MG_EV_READ) return;
    nyxj_cluster* c = conn->fn_data;
    if (!c->stopped && conn->recv.len > 0) {
        nyxj_parse_options o = nyxj_parse_options_default();
        o.max_depth = 8;
        o.max_input_size = 64 * 1024;
        nyxj_result r = nyxj_parse_ex((const char*)conn->recv.buf, conn->recv.len, &o);
        if (r.is_valid) {
            const char* t = nyxj_get_string(r.root, "t");
            if (t && strcmp(t, "nyxj-gossip") == 0) merge(c, r.root);
        }
        nyxj_free_value(r.root);
    }
    conn->recv.len = 0;
}

static nyxj_value* member_value(const member* m, bool self, uint64_t now) {
    nyxj_value* o = nyxj_new_object();
    if (!o) return NULL;
    nyxj_object_set_string(o, "id", m->id);
    nyxj_object_set_string(o, "name", m->name);
    nyxj_object_set_string(o, "gossip", m->gossip);
    nyxj_object_set_string(o, "http", m->http);
    nyxj_object_set_number(o, "hb", (double)m->heartbeat);
    nyxj_object_set_number(o, "load", m->load);
    if (now) {
        nyxj_object_set_string(o, "status", status_names[m->status]);
        nyxj_object_set_number(o, "last_seen_ms_ago", self ? 0 : (double)(now - m->last_seen_ms));
        nyxj_object_set_bool(o, "self", self);
    }
    return o;
}

static void send_to(nyxj_cluster* c, const char* addr, const char* payload, size_t len) {
    struct mg_connection* out = mg_connect(&c->app->mgr, addr, NULL, NULL);
    if (!out) return;
    mg_send(out, payload, len);
    out->is_draining = 1;
}

static double local_load(nyxj_cluster* c) {
    double load = 0;
    nyxj_pool* p = nyxj_app_pool(c->app);
    if (p) {
        nyxj_pool_stats ps;
        nyxj_pool_stats_get(p, &ps);
        load = ps.queue_capacity ? (double)ps.queued / ps.queue_capacity : 0;
        if (ps.threads) load += (double)ps.active / ps.threads;
    }
    return load + nyxj_app_eco_level(c->app);
}

static void gossip_tick(nyxj_app* app, void* user) {
    (void)app;
    nyxj_cluster* c = user;
    if (c->stopped) return;
    uint64_t now = nyxj_now_ms();
    c->self.heartbeat++;
    c->self.load = local_load(c);
    c->self.last_seen_ms = now;

    nyxj_value* msg = nyxj_new_object();
    nyxj_object_set_string(msg, "t", "nyxj-gossip");
    nyxj_object_set_number(msg, "v", 1);
    nyxj_value* list = nyxj_new_array();
    nyxj_array_push(list, member_value(&c->self, true, 0));

    const char* targets[3];
    int ntargets = 0, alive = 0;
    nyxj_mutex_lock(&c->lock);
    for (int i = 0; i < c->count; i++) {
        member* m = &c->members[i];
        uint64_t age = now - m->last_seen_ms;
        int st = age >= (uint64_t)c->dead_ms ? DEAD : age >= (uint64_t)c->suspect_ms ? SUSPECT : ALIVE;
        if (st != m->status) NYXJ_WARN(MOD, "peer %s is now %s", m->id, status_names[st]);
        m->status = st;
        if (st == DEAD) continue;
        nyxj_array_push(list, member_value(m, false, 0));
        if (st == ALIVE) alive++;
    }
    /* Pick up to three random alive peers as gossip targets. */
    for (int tries = 0; tries < 8 && ntargets < 3 && alive > 0; tries++) {
        uint32_t r = 0;
        mg_random(&r, sizeof(r));
        member* m = &c->members[r % (uint32_t)c->count];
        if (m->status != ALIVE || !m->gossip[0]) continue;
        bool dup = false;
        for (int k = 0; k < ntargets; k++) dup |= targets[k] == m->gossip;
        if (!dup) targets[ntargets++] = m->gossip;
    }
    nyxj_mutex_unlock(&c->lock);
    nyxj_object_set(msg, "members", list);
    size_t len = 0;
    char* payload = nyxj_serialize_ex(msg, NULL, &len);
    nyxj_free_value(msg);
    if (!payload) return;
    for (int k = 0; k < ntargets; k++) send_to(c, targets[k], payload, len);
    if (alive == 0 || c->self.heartbeat % 10 == 0) /* Re-join through seeds when isolated */
        for (int s = 0; s < c->nseeds; s++) send_to(c, c->seeds[s], payload, len);
    free(payload);
}

nyxj_cluster* nyxj_cluster_start(nyxj_app* app, const nyxj_cluster_config* cfg, nyxj_error* err) {
    nyxj_error_clear(err);
    if (!app || !cfg || !cfg->bind_url) { nyxj_error_set(err, NYXJ_ERR_NULL_INPUT, NULL); return NULL; }
    nyxj_cluster* c = calloc(1, sizeof(*c));
    if (!c) { nyxj_error_set(err, NYXJ_ERR_OUT_OF_MEMORY, NULL); return NULL; }
    c->app = app;
    nyxj_mutex_init(&c->lock);
    c->interval_ms = cfg->gossip_interval_ms > 0 ? cfg->gossip_interval_ms : 1000;
    c->suspect_ms = cfg->suspect_after_ms > 0 ? cfg->suspect_after_ms : 5000;
    c->dead_ms = cfg->dead_after_ms > 0 ? cfg->dead_after_ms : 15000;
    unsigned char rnd[8];
    mg_random(rnd, sizeof(rnd));
    for (int i = 0; i < 8; i++) snprintf(c->self.id + i * 2, 3, "%02x", rnd[i]);
    snprintf(c->self.name, sizeof(c->self.name), "%s", cfg->node_name ? cfg->node_name : c->self.id);
    snprintf(c->self.http, sizeof(c->self.http), "%s", cfg->http_addr ? cfg->http_addr : "");
    if (cfg->advertise_addr) {
        snprintf(c->self.gossip, sizeof(c->self.gossip), "%s", cfg->advertise_addr);
    } else { /* Derive a loopback address from the bind URL */
        const char* b = cfg->bind_url;
        const char* zero = strstr(b, "0.0.0.0");
        if (zero) snprintf(c->self.gossip, sizeof(c->self.gossip), "%.*s127.0.0.1%s", (int)(zero - b), b, zero + 7);
        else snprintf(c->self.gossip, sizeof(c->self.gossip), "%s", b);
    }
    if (cfg->seed_count > 0) {
        c->seeds = calloc((size_t)cfg->seed_count, sizeof(char*));
        for (int i = 0; c->seeds && i < cfg->seed_count; i++)
            if (cfg->seeds[i]) c->seeds[c->nseeds++] = nyxj_strdup(cfg->seeds[i]);
    }
    c->udp = mg_listen(&app->mgr, cfg->bind_url, udp_in, c);
    if (!c->udp) {
        nyxj_error_set(err, NYXJ_ERR_NETWORK, "cannot bind gossip socket on %s", cfg->bind_url);
        nyxj_cluster_free_internal(c);
        return NULL;
    }
    nyxj_app_add_timer(app, (unsigned)c->interval_ms, gossip_tick, c);
    if (app->cluster) nyxj_cluster_stop(app->cluster);
    app->cluster = c;
    NYXJ_INFO(MOD, "node %s (%s) gossiping on %s, %d seed(s)", c->self.id, c->self.name, c->self.gossip, c->nseeds);
    return c;
}

nyxj_value* nyxj_cluster_members(nyxj_cluster* c) {
    nyxj_value* arr = nyxj_new_array();
    if (!c || !arr) return arr;
    uint64_t now = nyxj_now_ms();
    c->self.last_seen_ms = now;
    nyxj_array_push(arr, member_value(&c->self, true, now));
    nyxj_mutex_lock(&c->lock);
    for (int i = 0; i < c->count; i++) nyxj_array_push(arr, member_value(&c->members[i], false, now));
    nyxj_mutex_unlock(&c->lock);
    return arr;
}

const char* nyxj_cluster_node_id(const nyxj_cluster* c) { return c ? c->self.id : NULL; }

int nyxj_cluster_alive_count(nyxj_cluster* c) {
    if (!c) return 0;
    int n = 1;
    nyxj_mutex_lock(&c->lock);
    for (int i = 0; i < c->count; i++) n += c->members[i].status == ALIVE;
    nyxj_mutex_unlock(&c->lock);
    return n;
}

bool nyxj_cluster_least_loaded(nyxj_cluster* c, char* http_addr, size_t n) {
    if (!c || !http_addr || !n) return false;
    bool found = false;
    double best = local_load(c);
    nyxj_mutex_lock(&c->lock);
    for (int i = 0; i < c->count; i++) {
        member* m = &c->members[i];
        if (m->status == ALIVE && m->http[0] && m->load < best) {
            best = m->load;
            snprintf(http_addr, n, "%s", m->http);
            found = true;
        }
    }
    nyxj_mutex_unlock(&c->lock);
    return found;
}

void nyxj_cluster_stop(nyxj_cluster* c) {
    if (!c || c->stopped) return;
    c->stopped = true;
    if (c->udp) c->udp->is_closing = 1;
    c->udp = NULL;
}

void nyxj_cluster_free_internal(void* p) {
    nyxj_cluster* c = p;
    if (!c) return;
    for (int i = 0; i < c->nseeds; i++) free(c->seeds[i]);
    free(c->seeds);
    free(c->members);
    nyxj_mutex_destroy(&c->lock);
    free(c);
}
