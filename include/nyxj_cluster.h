/*
 * Experimental decentralized clustering: a UDP gossip protocol through which
 * NyxJ instances discover each other without a central server, share their
 * load, and detect failed peers (alive -> suspect -> dead).
 */
#ifndef NYXJ_CLUSTER_H
#define NYXJ_CLUSTER_H

#include "nyxj_http.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* bind_url;        /* e.g. "udp://0.0.0.0:7946" */
    const char* advertise_addr;  /* Gossip address peers should use, e.g. "udp://10.0.0.5:7946" */
    const char* http_addr;       /* HTTP address announced to peers */
    const char* node_name;       /* Defaults to a random id */
    const char** seeds;          /* Gossip addresses to join, e.g. "udp://10.0.0.6:7946" */
    int seed_count;
    int gossip_interval_ms;      /* Default 1000 */
    int suspect_after_ms;        /* Default 5000 */
    int dead_after_ms;           /* Default 15000 */
} nyxj_cluster_config;

typedef struct nyxj_cluster nyxj_cluster;

nyxj_cluster* nyxj_cluster_start(nyxj_app* app, const nyxj_cluster_config* cfg, nyxj_error* err);
nyxj_value*   nyxj_cluster_members(nyxj_cluster* c);       /* Array of member objects */
const char*   nyxj_cluster_node_id(const nyxj_cluster* c);
int           nyxj_cluster_alive_count(nyxj_cluster* c);
bool          nyxj_cluster_least_loaded(nyxj_cluster* c, char* http_addr, size_t n); /* Best peer for offloading */
void          nyxj_cluster_stop(nyxj_cluster* c);

#ifdef __cplusplus
}
#endif

#endif
