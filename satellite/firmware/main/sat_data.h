/*
 * Local data model for the satellite display
 *
 * Stores the latest decoded network snapshot for the UI to read.
 */

#ifndef SAT_DATA_H
#define SAT_DATA_H

#include "sat_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#define SAT_DATA_HISTORY_LEN 60  /* 60 seconds of rate history */

typedef struct {
    char     name[SAT_IFACE_NAME_LEN + 1];
    bool     up;
    bool     running;
    bool     wireless;
    uint32_t rx_rate;       /* bytes/sec */
    uint32_t tx_rate;       /* bytes/sec */
    uint16_t link_speed;    /* Mbps */
    int8_t   signal_dbm;
    uint8_t  error_rate;
} sat_iface_t;

typedef struct {
    uint32_t  timestamp;
    uint32_t  uptime_sec;
    uint8_t   mem_used_pct;
    uint8_t   num_ifaces;
    sat_iface_t ifaces[SAT_MAX_INTERFACES];

    /* Connectivity */
    bool      gw_up;
    bool      dns_up;
    bool      dns_resolves;
    bool      wan_up;
    float     gw_latency_ms;
    float     dns_latency_ms;
    float     wan_latency_ms;
    float     wan_jitter_ms;
    uint8_t   wan_loss_pct;

    /* Alerts */
    uint8_t   active_alerts;
    uint8_t   max_severity;

    /* Aggregate rates (sum of all interfaces) */
    uint32_t  total_rx_rate;
    uint32_t  total_tx_rate;

    /* Max link speed across interfaces (for gauge scaling) */
    uint32_t  max_link_bps;

    /* RX/TX history ring buffers (aggregate, 1 sample/sec) */
    uint32_t  rx_history[SAT_DATA_HISTORY_LEN];
    uint32_t  tx_history[SAT_DATA_HISTORY_LEN];
    int       history_head;
    int       history_count;

    /* Connection state */
    bool      connected;
    uint32_t  last_rx_time;    /* millis of last received frame */
    uint16_t  last_seq;
    uint32_t  rx_frames;
    uint32_t  rx_errors;
} sat_data_t;

/* Get pointer to the shared data (read-only from UI, write from receiver) */
const sat_data_t *sat_data_get(void);

/* Update data from a decoded snapshot. Called by receiver. */
void sat_data_update_snapshot(const sat_snapshot_t *snap);

/* Update connection status. Called periodically. */
void sat_data_check_connection(uint32_t now_ms);

/* Mark connection as alive. Called on any valid frame receipt. */
void sat_data_mark_alive(uint32_t now_ms, uint16_t seq);

#endif /* SAT_DATA_H */
