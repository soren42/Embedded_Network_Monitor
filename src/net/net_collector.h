/**
 * @file net_collector.h
 * @brief Network data collection orchestrator
 */

#ifndef NET_COLLECTOR_H
#define NET_COLLECTOR_H

#include <stdint.h>
#include "app_conf.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per-interface statistics snapshot from /proc/net/dev */
typedef struct {
    char     name[APP_IFACE_NAME_MAX];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t collisions;
} net_stats_t;

/* Per-interface computed rates */
typedef struct {
    double rx_bytes_per_sec;
    double tx_bytes_per_sec;
    double rx_packets_per_sec;
    double tx_packets_per_sec;
    double rx_errors_per_sec;
    double tx_errors_per_sec;
} net_rates_t;

/* Per-interface info from ioctl */
typedef struct {
    char     name[APP_IFACE_NAME_MAX];
    int      is_up;
    int      is_running;
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t  mac[6];
    uint32_t speed_mbps;
    int      is_wireless;
} net_iface_info_t;

/* Connectivity probe results */
typedef struct {
    int    gateway_reachable;
    double gateway_latency_ms;
    int    dns_reachable;
    int    dns_resolves;
    double dns_latency_ms;
} net_connectivity_t;

/* WiFi specific info */
typedef struct {
    char   ssid[64];
    int    signal_dbm;
    int    link_quality;
    int    noise_dbm;
    double bitrate_mbps;
} net_wifi_info_t;

/* ARP table entry */
typedef struct {
    uint32_t ip_addr;
    uint8_t  mac[6];
    char     device[APP_IFACE_NAME_MAX];
} net_arp_entry_t;

/* TCP/UDP connection entry */
typedef struct {
    uint32_t local_addr;
    uint16_t local_port;
    uint32_t remote_addr;
    uint16_t remote_port;
    uint8_t  state;
    uint8_t  protocol; /* 6=TCP, 17=UDP */
} net_connection_t;

/* History sample for charts */
typedef struct {
    float rx_bps;
    float tx_bps;
} net_history_sample_t;

/* Complete state for one interface */
typedef struct {
    net_iface_info_t   info;
    net_stats_t        stats;
    net_rates_t        rates;
    net_wifi_info_t    wifi;

    /* Rate peaks since start */
    double peak_rx_bps;
    double peak_tx_bps;

    /* Daily totals */
    uint64_t rx_bytes_today;
    uint64_t tx_bytes_today;
} net_iface_state_t;

/* Global application network state */
typedef struct {
    int                num_ifaces;
    net_iface_state_t  ifaces[APP_MAX_INTERFACES];

    net_connectivity_t connectivity;

    int                num_arp;
    net_arp_entry_t    arp_table[APP_MAX_ARP_ENTRIES];

    int                num_connections;
    net_connection_t   connections[APP_MAX_CONNECTIONS];

    uint32_t           uptime_sec;
    uint32_t           mem_total_kb;
    uint32_t           mem_free_kb;
    uint32_t           mem_available_kb;
} net_state_t;

/**
 * Get the global network state (read-only).
 */
const net_state_t *net_get_state(void);

/**
 * Initialize the network collector and register LVGL timers.
 */
void net_collector_init(const config_t *cfg);

/**
 * Stop all collectors and free resources.
 */
void net_collector_shutdown(void);

/**
 * Get short history ring buffer for an interface (60s, 1s resolution).
 * @param iface_idx Interface index
 * @param out_data Pointer to receive data array pointer
 * @param out_count Pointer to receive count
 * @param out_head Pointer to receive head index
 * @param out_capacity Pointer to receive capacity
 */
void net_get_short_history(int iface_idx, const net_history_sample_t **out_data,
                           int *out_count, int *out_head, int *out_capacity);

/**
 * Get long history ring buffer for an interface (24h, 1-min resolution).
 */
void net_get_long_history(int iface_idx, const net_history_sample_t **out_data,
                          int *out_count, int *out_head, int *out_capacity);

#ifdef __cplusplus
}
#endif

#endif /* NET_COLLECTOR_H */
