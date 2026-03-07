/**
 * @file net_discovery.h
 * @brief WAN link discovery - next hop detection and link quality
 */

#ifndef NET_DISCOVERY_H
#define NET_DISCOVERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISCOVERY_MAX_HOPS 4

typedef struct {
    uint32_t ip_addr;
    double   latency_ms;
    int      reachable;
    char     ip_str[16];
} discovery_hop_t;

typedef struct {
    int              num_hops;
    discovery_hop_t  hops[DISCOVERY_MAX_HOPS];
    int              wan_reachable;       /* can reach beyond gateway */
    double           wan_latency_ms;      /* latency to first hop beyond gw */
    double           wan_jitter_ms;       /* jitter estimate */
    int              wan_packet_loss_pct; /* 0-100 */
} wan_discovery_t;

/**
 * Perform a lightweight traceroute to discover hops beyond the gateway.
 * Uses UDP probes with increasing TTL.
 *
 * @param gateway_ip  Gateway IP string (e.g. "10.0.0.1")
 * @param result      Output discovery result
 * @return 0 on success, -1 on error
 */
int discovery_trace_wan(const char *gateway_ip, wan_discovery_t *result);

/**
 * Quick WAN quality test - ping the next hop beyond gateway multiple times
 * to estimate jitter and packet loss.
 *
 * @param target_ip   IP of next hop (from trace)
 * @param result      Updated with jitter/loss stats
 * @return 0 on success, -1 on error
 */
int discovery_wan_quality(const char *target_ip, wan_discovery_t *result);

#ifdef __cplusplus
}
#endif

#endif /* NET_DISCOVERY_H */
