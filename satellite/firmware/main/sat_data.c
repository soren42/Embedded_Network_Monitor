/*
 * Local data model for satellite display
 */

#include "sat_data.h"
#include <string.h>

#define CONNECTION_TIMEOUT_MS 5000

static sat_data_t s_data;

const sat_data_t *sat_data_get(void)
{
    return &s_data;
}

void sat_data_update_snapshot(const sat_snapshot_t *snap)
{
    s_data.timestamp    = snap->timestamp;
    s_data.uptime_sec   = snap->uptime_sec;
    s_data.mem_used_pct = snap->mem_used_pct;

    /* Connectivity flags */
    s_data.gw_up         = (snap->conn_flags & SAT_FLAG_GATEWAY_UP) != 0;
    s_data.dns_up        = (snap->conn_flags & SAT_FLAG_DNS_UP) != 0;
    s_data.dns_resolves  = (snap->conn_flags & SAT_FLAG_DNS_RESOLVES) != 0;
    s_data.wan_up        = (snap->conn_flags & SAT_FLAG_WAN_UP) != 0;

    /* Latencies (stored as ms*10 in protocol) */
    s_data.gw_latency_ms  = snap->gw_latency / 10.0f;
    s_data.dns_latency_ms = snap->dns_latency / 10.0f;
    s_data.wan_latency_ms = snap->wan_latency / 10.0f;
    s_data.wan_jitter_ms  = snap->wan_jitter / 10.0f;
    s_data.wan_loss_pct   = snap->wan_loss_pct;

    /* Alerts */
    s_data.active_alerts = snap->active_alerts;
    s_data.max_severity  = snap->max_severity;

    /* Interfaces */
    int n = snap->num_ifaces;
    if (n > SAT_MAX_INTERFACES)
        n = SAT_MAX_INTERFACES;
    s_data.num_ifaces = (uint8_t)n;

    uint32_t total_rx = 0, total_tx = 0;
    uint32_t max_link = 0;

    for (int i = 0; i < n; i++) {
        const sat_iface_data_t *src = &snap->ifaces[i];
        sat_iface_t *dst = &s_data.ifaces[i];

        memcpy(dst->name, src->name, SAT_IFACE_NAME_LEN);
        dst->name[SAT_IFACE_NAME_LEN] = '\0';

        dst->up       = (src->flags & SAT_IFLAG_UP) != 0;
        dst->running  = (src->flags & SAT_IFLAG_RUNNING) != 0;
        dst->wireless = (src->flags & SAT_IFLAG_WIRELESS) != 0;

        dst->rx_rate    = src->rx_rate;
        dst->tx_rate    = src->tx_rate;
        dst->link_speed = src->link_speed;
        dst->signal_dbm = src->signal_dbm;
        dst->error_rate = src->error_rate;

        total_rx += src->rx_rate;
        total_tx += src->tx_rate;

        uint32_t link_bps = (uint32_t)src->link_speed * 1000000u;
        if (link_bps > max_link)
            max_link = link_bps;
    }

    s_data.total_rx_rate = total_rx;
    s_data.total_tx_rate = total_tx;
    s_data.max_link_bps  = max_link > 0 ? max_link : 100000000u; /* 100M default */

    /* Push to history ring buffer */
    s_data.rx_history[s_data.history_head] = total_rx;
    s_data.tx_history[s_data.history_head] = total_tx;
    s_data.history_head = (s_data.history_head + 1) % SAT_DATA_HISTORY_LEN;
    if (s_data.history_count < SAT_DATA_HISTORY_LEN)
        s_data.history_count++;
}

void sat_data_mark_alive(uint32_t now_ms, uint16_t seq)
{
    s_data.connected    = true;
    s_data.last_rx_time = now_ms;
    s_data.last_seq     = seq;
    s_data.rx_frames++;
}

void sat_data_check_connection(uint32_t now_ms)
{
    if (s_data.connected &&
        (now_ms - s_data.last_rx_time) > CONNECTION_TIMEOUT_MS) {
        s_data.connected = false;
    }
}
