/**
 * @file api_json.c
 * @brief JSON serialization of network state structs
 */

#include "api_json.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static cJSON *json_iface(const net_iface_state_t *is)
{
    cJSON *obj = cJSON_CreateObject();
    const net_iface_info_t *info = &is->info;
    const net_rates_t *rates = &is->rates;

    cJSON_AddStringToObject(obj, "name", info->name);
    cJSON_AddBoolToObject(obj, "up", info->is_up);
    cJSON_AddBoolToObject(obj, "running", info->is_running);
    cJSON_AddBoolToObject(obj, "wireless", info->is_wireless);
    cJSON_AddNumberToObject(obj, "speed_mbps", info->speed_mbps);

    char buf[32];
    format_ip(info->ip_addr, buf, sizeof(buf));
    cJSON_AddStringToObject(obj, "ip", buf);
    format_ip(info->netmask, buf, sizeof(buf));
    cJSON_AddStringToObject(obj, "netmask", buf);
    format_mac(info->mac, buf, sizeof(buf));
    cJSON_AddStringToObject(obj, "mac", buf);

    cJSON *stats = cJSON_AddObjectToObject(obj, "stats");
    cJSON_AddNumberToObject(stats, "rx_bytes", (double)is->stats.rx_bytes);
    cJSON_AddNumberToObject(stats, "tx_bytes", (double)is->stats.tx_bytes);
    cJSON_AddNumberToObject(stats, "rx_packets", (double)is->stats.rx_packets);
    cJSON_AddNumberToObject(stats, "tx_packets", (double)is->stats.tx_packets);
    cJSON_AddNumberToObject(stats, "rx_errors", (double)is->stats.rx_errors);
    cJSON_AddNumberToObject(stats, "tx_errors", (double)is->stats.tx_errors);
    cJSON_AddNumberToObject(stats, "rx_dropped", (double)is->stats.rx_dropped);
    cJSON_AddNumberToObject(stats, "tx_dropped", (double)is->stats.tx_dropped);

    cJSON *r = cJSON_AddObjectToObject(obj, "rates");
    cJSON_AddNumberToObject(r, "rx_bps", rates->rx_bytes_per_sec);
    cJSON_AddNumberToObject(r, "tx_bps", rates->tx_bytes_per_sec);
    cJSON_AddNumberToObject(r, "rx_pps", rates->rx_packets_per_sec);
    cJSON_AddNumberToObject(r, "tx_pps", rates->tx_packets_per_sec);

    cJSON_AddNumberToObject(obj, "peak_rx_bps", is->peak_rx_bps);
    cJSON_AddNumberToObject(obj, "peak_tx_bps", is->peak_tx_bps);
    cJSON_AddNumberToObject(obj, "rx_bytes_today", (double)is->rx_bytes_today);
    cJSON_AddNumberToObject(obj, "tx_bytes_today", (double)is->tx_bytes_today);

    if (info->is_wireless) {
        cJSON *wifi = cJSON_AddObjectToObject(obj, "wifi");
        cJSON_AddStringToObject(wifi, "ssid", is->wifi.ssid);
        cJSON_AddNumberToObject(wifi, "signal_dbm", is->wifi.signal_dbm);
        cJSON_AddNumberToObject(wifi, "link_quality", is->wifi.link_quality);
        cJSON_AddNumberToObject(wifi, "noise_dbm", is->wifi.noise_dbm);
        cJSON_AddNumberToObject(wifi, "bitrate_mbps", is->wifi.bitrate_mbps);
    }

    return obj;
}

cJSON *api_json_interfaces(const net_state_t *state)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < state->num_ifaces; i++) {
        cJSON_AddItemToArray(arr, json_iface(&state->ifaces[i]));
    }
    return arr;
}

cJSON *api_json_connectivity(const net_state_t *state)
{
    const net_connectivity_t *c = &state->connectivity;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "gateway_reachable", c->gateway_reachable);
    cJSON_AddNumberToObject(obj, "gateway_latency_ms", c->gateway_latency_ms);
    cJSON_AddBoolToObject(obj, "dns_reachable", c->dns_reachable);
    cJSON_AddBoolToObject(obj, "dns_resolves", c->dns_resolves);
    cJSON_AddNumberToObject(obj, "dns_latency_ms", c->dns_latency_ms);
    return obj;
}

cJSON *api_json_connections(const net_state_t *state)
{
    cJSON *arr = cJSON_CreateArray();
    char buf[32];
    for (int i = 0; i < state->num_connections; i++) {
        const net_connection_t *conn = &state->connections[i];
        cJSON *obj = cJSON_CreateObject();

        format_ip(conn->local_addr, buf, sizeof(buf));
        cJSON_AddStringToObject(obj, "local_addr", buf);
        cJSON_AddNumberToObject(obj, "local_port", conn->local_port);

        format_ip(conn->remote_addr, buf, sizeof(buf));
        cJSON_AddStringToObject(obj, "remote_addr", buf);
        cJSON_AddNumberToObject(obj, "remote_port", conn->remote_port);

        cJSON_AddStringToObject(obj, "protocol",
                                conn->protocol == 6 ? "TCP" : "UDP");
        cJSON_AddNumberToObject(obj, "state", conn->state);

        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

cJSON *api_json_alerts(void)
{
    const alert_entry_t *entries;
    int count;
    int active = alerts_get_log(&entries, &count);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "active_count", active);

    cJSON *arr = cJSON_AddArrayToObject(obj, "entries");
    for (int i = 0; i < count; i++) {
        const alert_entry_t *a = &entries[i];
        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "type", a->type);
        cJSON_AddNumberToObject(e, "severity", a->severity);
        cJSON_AddNumberToObject(e, "timestamp", a->timestamp);
        cJSON_AddStringToObject(e, "interface", a->iface);
        cJSON_AddStringToObject(e, "message", a->message);
        cJSON_AddBoolToObject(e, "active", a->active);
        cJSON_AddItemToArray(arr, e);
    }
    return obj;
}

cJSON *api_json_infra(const net_state_t *state)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < state->num_infra; i++) {
        const infra_device_t *dev = &state->infra[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", dev->name);
        cJSON_AddStringToObject(obj, "ip", dev->ip_str);
        cJSON_AddNumberToObject(obj, "type", dev->type);
        cJSON_AddBoolToObject(obj, "enabled", dev->enabled);
        cJSON_AddBoolToObject(obj, "reachable", dev->reachable);
        cJSON_AddNumberToObject(obj, "latency_ms", dev->latency_ms);
        cJSON_AddNumberToObject(obj, "consecutive_fails", dev->consecutive_fails);
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

cJSON *api_json_wan(const wan_discovery_t *wan)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "wan_reachable", wan->wan_reachable);
    cJSON_AddNumberToObject(obj, "wan_latency_ms", wan->wan_latency_ms);
    cJSON_AddNumberToObject(obj, "wan_jitter_ms", wan->wan_jitter_ms);
    cJSON_AddNumberToObject(obj, "wan_packet_loss_pct", wan->wan_packet_loss_pct);
    cJSON_AddNumberToObject(obj, "num_hops", wan->num_hops);

    cJSON *hops = cJSON_AddArrayToObject(obj, "hops");
    for (int i = 0; i < wan->num_hops; i++) {
        const discovery_hop_t *h = &wan->hops[i];
        cJSON *hop = cJSON_CreateObject();
        cJSON_AddStringToObject(hop, "ip", h->ip_str);
        cJSON_AddNumberToObject(hop, "latency_ms", h->latency_ms);
        cJSON_AddBoolToObject(hop, "reachable", h->reachable);
        cJSON_AddItemToArray(hops, hop);
    }
    return obj;
}

cJSON *api_json_config(const config_t *cfg)
{
    cJSON *obj = cJSON_CreateObject();
    for (int i = 0; i < cfg->count; i++) {
        cJSON_AddStringToObject(obj, cfg->entries[i].key,
                                cfg->entries[i].value);
    }
    return obj;
}

cJSON *api_json_info(const net_state_t *state)
{
    cJSON *obj = cJSON_CreateObject();

    char hostname[64] = "";
    gethostname(hostname, sizeof(hostname));
    cJSON_AddStringToObject(obj, "hostname", hostname);
    cJSON_AddStringToObject(obj, "version", "3.0.0");
    cJSON_AddNumberToObject(obj, "uptime_sec", state->uptime_sec);
    cJSON_AddNumberToObject(obj, "mem_total_kb", state->mem_total_kb);
    cJSON_AddNumberToObject(obj, "mem_free_kb", state->mem_free_kb);
    cJSON_AddNumberToObject(obj, "mem_available_kb", state->mem_available_kb);

    return obj;
}

cJSON *api_json_status(const net_state_t *state)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "info", api_json_info(state));
    cJSON_AddItemToObject(obj, "interfaces", api_json_interfaces(state));
    cJSON_AddItemToObject(obj, "connectivity", api_json_connectivity(state));
    cJSON_AddItemToObject(obj, "alerts", api_json_alerts());
    cJSON_AddItemToObject(obj, "infra", api_json_infra(state));
    cJSON_AddItemToObject(obj, "wan", api_json_wan(net_get_wan_discovery()));
    return obj;
}
