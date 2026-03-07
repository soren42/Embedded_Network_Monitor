/**
 * @file net_collector.c
 * @brief Network data collection orchestrator
 *
 * Reads /proc/net/dev, enumerates interfaces via ioctl, computes
 * traffic rates, and manages history ring buffers. Registers LVGL
 * timers for periodic collection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>

#include "lvgl.h"
#include "net_collector.h"
#include "net_interfaces.h"
#include "net_ping.h"
#include "net_dns.h"
#include "net_arp.h"
#include "net_wifi.h"
#include "net_wifi_mgr.h"
#include "net_connections.h"
#include "net_infra.h"
#include "net_discovery.h"
#include "alerts.h"
#include "log.h"

/* History ring buffers */
static net_history_sample_t g_short_history[APP_MAX_INTERFACES][APP_HISTORY_SHORT_LEN];
static int g_short_head[APP_MAX_INTERFACES];
static int g_short_count[APP_MAX_INTERFACES];

static net_history_sample_t g_long_history[APP_MAX_INTERFACES][APP_HISTORY_LONG_LEN];
static int g_long_head[APP_MAX_INTERFACES];
static int g_long_count[APP_MAX_INTERFACES];

/* Previous stats snapshot for rate computation */
static net_stats_t g_prev_stats[APP_MAX_INTERFACES];
static int g_prev_valid;
static struct timespec g_prev_time;

/* Global state */
static net_state_t g_state;
static config_t *g_cfg;

/* Minute accumulation for long history */
static float g_minute_rx_acc[APP_MAX_INTERFACES];
static float g_minute_tx_acc[APP_MAX_INTERFACES];
static int g_minute_sample_count;

/* Infrastructure cycling index */
static int g_infra_ping_idx;

/* WAN discovery state */
static wan_discovery_t g_wan_discovery;
static int g_wan_discovery_tick;

/* Hidden interface prefixes (filtered from UI) */
static const char *g_hidden_iface_prefixes[] = { "usb", NULL };

/* WiFi auto-connect state */
static int g_wifi_auto_connected;

/* Promiscuous mode setting */
static int g_promiscuous_enabled;

/* LVGL timer handles */
static lv_timer_t *g_fast_timer;
static lv_timer_t *g_medium_timer;
static lv_timer_t *g_slow_timer;

/**
 * Load infrastructure device configuration.
 */
static void load_infra_config(void)
{
    int count = config_get_int(g_cfg, "infra_count", 0);
    if (count > APP_MAX_INFRA_DEVICES) count = APP_MAX_INFRA_DEVICES;
    g_state.num_infra = count;

    for (int i = 0; i < count; i++) {
        char key[64];
        infra_device_t *dev = &g_state.infra[i];

        snprintf(key, sizeof(key), "infra_%d_name", i);
        const char *name = config_get_str(g_cfg, key, "Device");
        strncpy(dev->name, name, sizeof(dev->name) - 1);
        dev->name[sizeof(dev->name) - 1] = '\0';

        snprintf(key, sizeof(key), "infra_%d_ip", i);
        const char *ip = config_get_str(g_cfg, key, "");
        strncpy(dev->ip_str, ip, sizeof(dev->ip_str) - 1);
        dev->ip_str[sizeof(dev->ip_str) - 1] = '\0';

        snprintf(key, sizeof(key), "infra_%d_type", i);
        const char *type = config_get_str(g_cfg, key, "other");
        dev->type = infra_parse_type(type);

        dev->enabled = (dev->ip_str[0] != '\0') ? 1 : 0;
        dev->reachable = 0;
        dev->latency_ms = 0.0;
        dev->consecutive_fails = 0;

        if (dev->enabled) {
            LOG_INFO("Infra device %d: %s (%s)", i, dev->name, dev->ip_str);
        }
    }
    g_infra_ping_idx = 0;
}

/**
 * Apply promiscuous mode setting to all active non-loopback interfaces.
 */
static void apply_promiscuous_mode(void)
{
    if (!g_promiscuous_enabled) return;

    for (int i = 0; i < g_state.num_ifaces; i++) {
        const char *name = g_state.ifaces[i].info.name;
        if (net_set_promiscuous(name, 1) == 0) {
            LOG_INFO("Promiscuous mode enabled on %s", name);
        }
    }
}

const net_state_t *net_get_state(void)
{
    return &g_state;
}

net_state_t *net_get_state_mut(void)
{
    return &g_state;
}

config_t *net_get_config(void)
{
    return (config_t *)g_cfg;
}

const wan_discovery_t *net_get_wan_discovery(void)
{
    return &g_wan_discovery;
}

int net_iface_is_hidden(const char *name)
{
    if (!name) return 0;
    for (int i = 0; g_hidden_iface_prefixes[i]; i++) {
        if (strncmp(name, g_hidden_iface_prefixes[i],
                    strlen(g_hidden_iface_prefixes[i])) == 0)
            return 1;
    }
    return 0;
}

void net_get_short_history(int iface_idx, const net_history_sample_t **out_data,
                           int *out_count, int *out_head, int *out_capacity)
{
    if (iface_idx < 0 || iface_idx >= APP_MAX_INTERFACES) {
        *out_data = NULL;
        *out_count = 0;
        return;
    }
    *out_data = g_short_history[iface_idx];
    *out_count = g_short_count[iface_idx];
    *out_head = g_short_head[iface_idx];
    *out_capacity = APP_HISTORY_SHORT_LEN;
}

void net_get_long_history(int iface_idx, const net_history_sample_t **out_data,
                          int *out_count, int *out_head, int *out_capacity)
{
    if (iface_idx < 0 || iface_idx >= APP_MAX_INTERFACES) {
        *out_data = NULL;
        *out_count = 0;
        return;
    }
    *out_data = g_long_history[iface_idx];
    *out_count = g_long_count[iface_idx];
    *out_head = g_long_head[iface_idx];
    *out_capacity = APP_HISTORY_LONG_LEN;
}

/**
 * Parse /proc/net/dev and fill stats array.
 * @return number of interfaces parsed
 */
static int parse_proc_net_dev(net_stats_t *stats, int max_count)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) {
        return 0;
    }

    char line[512];
    int count = 0;

    /* Skip two header lines */
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }
    if (!fgets(line, sizeof(line), fp)) { fclose(fp); return 0; }

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        char *colon = strchr(line, ':');
        if (!colon) continue;

        /* Extract interface name */
        char *name_start = line;
        while (*name_start == ' ') name_start++;
        int name_len = (int)(colon - name_start);
        if (name_len >= APP_IFACE_NAME_MAX) name_len = APP_IFACE_NAME_MAX - 1;
        memcpy(stats[count].name, name_start, name_len);
        stats[count].name[name_len] = '\0';

        /* Skip loopback */
        if (strcmp(stats[count].name, "lo") == 0) continue;

        /* Parse counters after the colon */
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop;
        uint64_t rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes, tx_packets, tx_errs, tx_drop;
        uint64_t tx_fifo, tx_colls, tx_carrier, tx_compressed;

        int n = sscanf(colon + 1,
            " %llu %llu %llu %llu %llu %llu %llu %llu"
            " %llu %llu %llu %llu %llu %llu %llu %llu",
            (unsigned long long *)&rx_bytes,
            (unsigned long long *)&rx_packets,
            (unsigned long long *)&rx_errs,
            (unsigned long long *)&rx_drop,
            (unsigned long long *)&rx_fifo,
            (unsigned long long *)&rx_frame,
            (unsigned long long *)&rx_compressed,
            (unsigned long long *)&rx_multicast,
            (unsigned long long *)&tx_bytes,
            (unsigned long long *)&tx_packets,
            (unsigned long long *)&tx_errs,
            (unsigned long long *)&tx_drop,
            (unsigned long long *)&tx_fifo,
            (unsigned long long *)&tx_colls,
            (unsigned long long *)&tx_carrier,
            (unsigned long long *)&tx_compressed);

        if (n >= 16) {
            stats[count].rx_bytes   = rx_bytes;
            stats[count].tx_bytes   = tx_bytes;
            stats[count].rx_packets = rx_packets;
            stats[count].tx_packets = tx_packets;
            stats[count].rx_errors  = rx_errs;
            stats[count].tx_errors  = tx_errs;
            stats[count].rx_dropped = rx_drop;
            stats[count].tx_dropped = tx_drop;
            stats[count].collisions = tx_colls;
            count++;
        }
    }

    fclose(fp);
    return count;
}

/**
 * Read system memory info from /proc/meminfo.
 */
static void read_meminfo(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) return;

    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long val;
        if (sscanf(line, "MemTotal: %lu kB", &val) == 1)
            g_state.mem_total_kb = (uint32_t)val;
        else if (sscanf(line, "MemFree: %lu kB", &val) == 1)
            g_state.mem_free_kb = (uint32_t)val;
        else if (sscanf(line, "MemAvailable: %lu kB", &val) == 1)
            g_state.mem_available_kb = (uint32_t)val;
    }
    fclose(fp);
}

/**
 * Push a sample into a history ring buffer.
 */
static void history_push(net_history_sample_t *buf, int *head, int *count,
                         int capacity, float rx, float tx)
{
    buf[*head].rx_bps = rx;
    buf[*head].tx_bps = tx;
    *head = (*head + 1) % capacity;
    if (*count < capacity) (*count)++;
}

/**
 * Fast timer callback (1s): collect stats, compute rates, update short history.
 */
static void fast_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Read current stats */
    net_stats_t cur_stats[APP_MAX_INTERFACES];
    int cur_count = parse_proc_net_dev(cur_stats, APP_MAX_INTERFACES);

    /* Get current time */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Update system info */
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        g_state.uptime_sec = (uint32_t)si.uptime;
    }
    read_meminfo();

    /* Compute rates if we have a previous sample */
    if (g_prev_valid) {
        double dt = (now.tv_sec - g_prev_time.tv_sec) +
                    (now.tv_nsec - g_prev_time.tv_nsec) / 1e9;
        if (dt > 0.0) {
            for (int i = 0; i < cur_count; i++) {
                /* Find this interface in the state array (single lookup) */
                int si_idx = -1;
                for (int k = 0; k < g_state.num_ifaces; k++) {
                    if (strcmp(g_state.ifaces[k].info.name,
                              cur_stats[i].name) == 0) {
                        si_idx = k;
                        break;
                    }
                }
                if (si_idx < 0) continue;

                /* Find matching previous stats entry */
                int prev_idx = -1;
                for (int j = 0; j < g_state.num_ifaces; j++) {
                    if (strcmp(g_prev_stats[j].name,
                              cur_stats[i].name) == 0) {
                        prev_idx = j;
                        break;
                    }
                }
                if (prev_idx < 0) continue;

                net_iface_state_t *st = &g_state.ifaces[si_idx];

                uint64_t drx = cur_stats[i].rx_bytes - g_prev_stats[prev_idx].rx_bytes;
                uint64_t dtx = cur_stats[i].tx_bytes - g_prev_stats[prev_idx].tx_bytes;

                st->rates.rx_bytes_per_sec = drx / dt;
                st->rates.tx_bytes_per_sec = dtx / dt;
                st->rates.rx_packets_per_sec =
                    (cur_stats[i].rx_packets - g_prev_stats[prev_idx].rx_packets) / dt;
                st->rates.tx_packets_per_sec =
                    (cur_stats[i].tx_packets - g_prev_stats[prev_idx].tx_packets) / dt;
                st->rates.rx_errors_per_sec =
                    (cur_stats[i].rx_errors - g_prev_stats[prev_idx].rx_errors) / dt;
                st->rates.tx_errors_per_sec =
                    (cur_stats[i].tx_errors - g_prev_stats[prev_idx].tx_errors) / dt;

                /* Update peaks */
                if (st->rates.rx_bytes_per_sec > st->peak_rx_bps)
                    st->peak_rx_bps = st->rates.rx_bytes_per_sec;
                if (st->rates.tx_bytes_per_sec > st->peak_tx_bps)
                    st->peak_tx_bps = st->rates.tx_bytes_per_sec;

                /* Update daily totals */
                st->rx_bytes_today += drx;
                st->tx_bytes_today += dtx;

                /* Push short history */
                history_push(g_short_history[si_idx],
                             &g_short_head[si_idx], &g_short_count[si_idx],
                             APP_HISTORY_SHORT_LEN,
                             (float)st->rates.rx_bytes_per_sec,
                             (float)st->rates.tx_bytes_per_sec);

                /* Accumulate for long history */
                g_minute_rx_acc[si_idx] += (float)st->rates.rx_bytes_per_sec;
                g_minute_tx_acc[si_idx] += (float)st->rates.tx_bytes_per_sec;
            }
        }
    }

    /* Long history: push 1-minute average every 60 samples */
    g_minute_sample_count++;
    if (g_minute_sample_count >= 60) {
        for (int i = 0; i < g_state.num_ifaces; i++) {
            float avg_rx = g_minute_rx_acc[i] / 60.0f;
            float avg_tx = g_minute_tx_acc[i] / 60.0f;
            history_push(g_long_history[i], &g_long_head[i], &g_long_count[i],
                         APP_HISTORY_LONG_LEN, avg_rx, avg_tx);
            g_minute_rx_acc[i] = 0.0f;
            g_minute_tx_acc[i] = 0.0f;
        }
        g_minute_sample_count = 0;
    }

    /* Update raw stats in state */
    for (int i = 0; i < cur_count; i++) {
        for (int k = 0; k < g_state.num_ifaces; k++) {
            if (strcmp(g_state.ifaces[k].info.name, cur_stats[i].name) == 0) {
                g_state.ifaces[k].stats = cur_stats[i];
                break;
            }
        }
    }

    /* Save current as previous */
    memcpy(g_prev_stats, cur_stats, sizeof(net_stats_t) * cur_count);
    g_prev_time = now;
    g_prev_valid = 1;
}

/**
 * Medium timer callback (5s): ping, connectivity checks.
 */
static void medium_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Ping gateway */
    const char *ping_target = config_get_str(g_cfg, "ping_target",
                                              APP_DEFAULT_PING_TARGET);
    double latency = 0.0;
    int reachable = net_ping(ping_target, 2000, &latency);
    g_state.connectivity.gateway_reachable = reachable;
    g_state.connectivity.gateway_latency_ms = latency;

    /* Collect active connections */
    g_state.num_connections = net_collect_connections(
        g_state.connections, APP_MAX_CONNECTIONS);

    /* Check WiFi interfaces */
    for (int i = 0; i < g_state.num_ifaces; i++) {
        if (g_state.ifaces[i].info.is_wireless) {
            net_wifi_read(g_state.ifaces[i].info.name, &g_state.ifaces[i].wifi);
        }
    }

    /* Ping one infrastructure device per tick (staggered) */
    if (g_state.num_infra > 0) {
        /* Find next enabled device */
        for (int attempts = 0; attempts < g_state.num_infra; attempts++) {
            int idx = g_infra_ping_idx % g_state.num_infra;
            g_infra_ping_idx = (g_infra_ping_idx + 1) % g_state.num_infra;
            infra_device_t *dev = &g_state.infra[idx];
            if (!dev->enabled) continue;

            double lat = 0.0;
            int ok = net_ping(dev->ip_str, 1000, &lat);
            dev->reachable = ok;
            dev->latency_ms = lat;
            if (ok) {
                dev->consecutive_fails = 0;
            } else {
                dev->consecutive_fails++;
            }
            break; /* only ping one per tick */
        }
    }

    /* Run alert checks */
    alerts_check(&g_state);
}

/**
 * Slow timer callback (30s): DNS, ARP, interface re-enumeration.
 */
static void slow_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Re-enumerate interfaces */
    net_iface_info_t iface_list[APP_MAX_INTERFACES];
    int count = net_enumerate_interfaces(iface_list, APP_MAX_INTERFACES);

    /* Update or add interfaces */
    for (int i = 0; i < count; i++) {
        int found = 0;
        for (int j = 0; j < g_state.num_ifaces; j++) {
            if (strcmp(g_state.ifaces[j].info.name, iface_list[i].name) == 0) {
                g_state.ifaces[j].info = iface_list[i];
                found = 1;
                break;
            }
        }
        if (!found && g_state.num_ifaces < APP_MAX_INTERFACES) {
            memset(&g_state.ifaces[g_state.num_ifaces], 0,
                   sizeof(net_iface_state_t));
            g_state.ifaces[g_state.num_ifaces].info = iface_list[i];
            g_state.num_ifaces++;
            /* Enable promiscuous mode on newly discovered interface */
            if (g_promiscuous_enabled) {
                net_set_promiscuous(iface_list[i].name, 1);
            }
        }
    }

    /* DNS probe */
    const char *dns_target = config_get_str(g_cfg, "dns_target",
                                             APP_DEFAULT_DNS_TARGET);
    const char *dns_host = config_get_str(g_cfg, "dns_test_hostname",
                                           APP_DEFAULT_DNS_HOST);
    double dns_lat = 0.0;
    int dns_ok = net_dns_probe(dns_target, dns_host, 3000, &dns_lat);
    g_state.connectivity.dns_reachable = dns_ok;
    g_state.connectivity.dns_resolves = dns_ok;
    g_state.connectivity.dns_latency_ms = dns_lat;

    /* ARP table */
    g_state.num_arp = net_read_arp(g_state.arp_table, APP_MAX_ARP_ENTRIES);

    /* WAN discovery - run every other slow tick (~60s) */
    g_wan_discovery_tick++;
    if (g_wan_discovery_tick >= 2) {
        g_wan_discovery_tick = 0;
        const char *gw = config_get_str(g_cfg, "ping_target",
                                         APP_DEFAULT_PING_TARGET);
        discovery_trace_wan(gw, &g_wan_discovery);

        /* Quality test if we found a next hop */
        if (g_wan_discovery.num_hops >= 2 &&
            g_wan_discovery.hops[1].reachable) {
            discovery_wan_quality(g_wan_discovery.hops[1].ip_str,
                                  &g_wan_discovery);
        }
    }

    /* WiFi manager status update */
    wifi_mgr_update();
}

void net_collector_init(config_t *cfg)
{
    g_cfg = cfg;
    memset(&g_state, 0, sizeof(g_state));
    memset(g_short_history, 0, sizeof(g_short_history));
    memset(g_long_history, 0, sizeof(g_long_history));
    memset(g_prev_stats, 0, sizeof(g_prev_stats));
    memset(&g_wan_discovery, 0, sizeof(g_wan_discovery));
    g_prev_valid = 0;
    g_minute_sample_count = 0;
    g_wan_discovery_tick = 0;
    g_wifi_auto_connected = 0;

    /* Load infrastructure device config */
    load_infra_config();

    /* Initialize WiFi manager */
    if (wifi_mgr_init() == 0) {
        /* Auto-connect if configured and not already connected */
        const wifi_mgr_state_t *ws = wifi_mgr_get_state();
        if (!ws->connected) {
            const char *wifi_ssid = config_get_str(cfg, "wifi_ssid", "");
            const char *wifi_pass = config_get_str(cfg, "wifi_password", "");
            int wifi_enabled = config_get_int(cfg, "wifi_enabled", 1);
            if (wifi_enabled && wifi_ssid[0] != '\0') {
                LOG_INFO("Auto-connecting to WiFi: %s", wifi_ssid);
                wifi_mgr_connect(wifi_ssid, wifi_pass);
                g_wifi_auto_connected = 1;
            }
        }
    }

    /* Initial interface enumeration */
    net_iface_info_t iface_list[APP_MAX_INTERFACES];
    int count = net_enumerate_interfaces(iface_list, APP_MAX_INTERFACES);
    for (int i = 0; i < count && i < APP_MAX_INTERFACES; i++) {
        g_state.ifaces[i].info = iface_list[i];
    }
    g_state.num_ifaces = count;

    /* Enable promiscuous mode if configured */
    g_promiscuous_enabled = config_get_int(cfg, "promiscuous_mode",
                                            APP_DEFAULT_PROMISCUOUS);
    if (g_promiscuous_enabled) {
        apply_promiscuous_mode();
    }

    /* Initial stats read */
    net_stats_t initial_stats[APP_MAX_INTERFACES];
    int stat_count = parse_proc_net_dev(initial_stats, APP_MAX_INTERFACES);
    for (int i = 0; i < stat_count; i++) {
        for (int k = 0; k < g_state.num_ifaces; k++) {
            if (strcmp(g_state.ifaces[k].info.name, initial_stats[i].name) == 0) {
                g_state.ifaces[k].stats = initial_stats[i];
                break;
            }
        }
    }
    memcpy(g_prev_stats, initial_stats, sizeof(net_stats_t) * stat_count);
    clock_gettime(CLOCK_MONOTONIC, &g_prev_time);

    /* Register LVGL timers */
    g_fast_timer = lv_timer_create(fast_timer_cb, APP_FAST_TIMER_MS, NULL);
    g_medium_timer = lv_timer_create(medium_timer_cb, APP_MEDIUM_TIMER_MS, NULL);
    g_slow_timer = lv_timer_create(slow_timer_cb, APP_SLOW_TIMER_MS, NULL);

    LOG_INFO("Network collector initialized with %d interfaces", count);
}

void net_collector_shutdown(void)
{
    if (g_fast_timer) lv_timer_del(g_fast_timer);
    if (g_medium_timer) lv_timer_del(g_medium_timer);
    if (g_slow_timer) lv_timer_del(g_slow_timer);
    g_fast_timer = NULL;
    g_medium_timer = NULL;
    g_slow_timer = NULL;
}
