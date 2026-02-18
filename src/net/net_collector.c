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
#include "net_connections.h"
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
static const config_t *g_cfg;

/* Minute accumulation for long history */
static float g_minute_rx_acc[APP_MAX_INTERFACES];
static float g_minute_tx_acc[APP_MAX_INTERFACES];
static int g_minute_sample_count;

/* LVGL timer handles */
static lv_timer_t *g_fast_timer;
static lv_timer_t *g_medium_timer;
static lv_timer_t *g_slow_timer;

const net_state_t *net_get_state(void)
{
    return &g_state;
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
                /* Find matching previous interface */
                for (int j = 0; j < g_state.num_ifaces; j++) {
                    if (strcmp(cur_stats[i].name, g_prev_stats[j].name) == 0) {
                        net_iface_state_t *st = NULL;
                        /* Find matching state entry */
                        for (int k = 0; k < g_state.num_ifaces; k++) {
                            if (strcmp(g_state.ifaces[k].info.name, cur_stats[i].name) == 0) {
                                st = &g_state.ifaces[k];
                                break;
                            }
                        }
                        if (!st) break;

                        uint64_t drx = cur_stats[i].rx_bytes - g_prev_stats[j].rx_bytes;
                        uint64_t dtx = cur_stats[i].tx_bytes - g_prev_stats[j].tx_bytes;

                        st->rates.rx_bytes_per_sec = drx / dt;
                        st->rates.tx_bytes_per_sec = dtx / dt;
                        st->rates.rx_packets_per_sec =
                            (cur_stats[i].rx_packets - g_prev_stats[j].rx_packets) / dt;
                        st->rates.tx_packets_per_sec =
                            (cur_stats[i].tx_packets - g_prev_stats[j].tx_packets) / dt;
                        st->rates.rx_errors_per_sec =
                            (cur_stats[i].rx_errors - g_prev_stats[j].rx_errors) / dt;
                        st->rates.tx_errors_per_sec =
                            (cur_stats[i].tx_errors - g_prev_stats[j].tx_errors) / dt;

                        /* Update peaks */
                        if (st->rates.rx_bytes_per_sec > st->peak_rx_bps)
                            st->peak_rx_bps = st->rates.rx_bytes_per_sec;
                        if (st->rates.tx_bytes_per_sec > st->peak_tx_bps)
                            st->peak_tx_bps = st->rates.tx_bytes_per_sec;

                        /* Update daily totals */
                        st->rx_bytes_today += drx;
                        st->tx_bytes_today += dtx;

                        /* Push short history */
                        for (int k = 0; k < g_state.num_ifaces; k++) {
                            if (strcmp(g_state.ifaces[k].info.name, cur_stats[i].name) == 0) {
                                history_push(g_short_history[k],
                                             &g_short_head[k], &g_short_count[k],
                                             APP_HISTORY_SHORT_LEN,
                                             (float)st->rates.rx_bytes_per_sec,
                                             (float)st->rates.tx_bytes_per_sec);

                                /* Accumulate for long history */
                                g_minute_rx_acc[k] += (float)st->rates.rx_bytes_per_sec;
                                g_minute_tx_acc[k] += (float)st->rates.tx_bytes_per_sec;
                                break;
                            }
                        }
                        break;
                    }
                }
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
}

void net_collector_init(const config_t *cfg)
{
    g_cfg = cfg;
    memset(&g_state, 0, sizeof(g_state));
    memset(g_short_history, 0, sizeof(g_short_history));
    memset(g_long_history, 0, sizeof(g_long_history));
    memset(g_prev_stats, 0, sizeof(g_prev_stats));
    g_prev_valid = 0;
    g_minute_sample_count = 0;

    /* Initial interface enumeration */
    net_iface_info_t iface_list[APP_MAX_INTERFACES];
    int count = net_enumerate_interfaces(iface_list, APP_MAX_INTERFACES);
    for (int i = 0; i < count && i < APP_MAX_INTERFACES; i++) {
        g_state.ifaces[i].info = iface_list[i];
    }
    g_state.num_ifaces = count;

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
