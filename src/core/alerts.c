/**
 * @file alerts.c
 * @brief Alert threshold engine with hysteresis
 */

#include <string.h>
#include <time.h>
#include "alerts.h"
#include "net_collector.h"
#include "log.h"

static alert_entry_t g_alert_log[APP_MAX_ALERTS];
static int g_alert_count;
static int g_alert_head;
static int g_active_count;

/* Hysteresis counters per interface per alert type */
static int g_hyst_counters[APP_MAX_INTERFACES][ALERT_TYPE_COUNT];

static const config_t *g_cfg;

static void push_alert(alert_type_t type, alert_severity_t sev,
                        const char *iface, const char *msg)
{
    alert_entry_t *e = &g_alert_log[g_alert_head];
    e->type = type;
    e->severity = sev;
    e->timestamp = (uint32_t)time(NULL);
    if (iface) {
        strncpy(e->iface, iface, APP_IFACE_NAME_MAX - 1);
        e->iface[APP_IFACE_NAME_MAX - 1] = '\0';
    } else {
        e->iface[0] = '\0';
    }
    strncpy(e->message, msg, sizeof(e->message) - 1);
    e->message[sizeof(e->message) - 1] = '\0';
    e->active = 1;

    g_alert_head = (g_alert_head + 1) % APP_MAX_ALERTS;
    if (g_alert_count < APP_MAX_ALERTS) g_alert_count++;
    g_active_count++;

    LOG_WARN("ALERT [%s] %s: %s",
             sev == ALERT_SEV_CRITICAL ? "CRIT" :
             sev == ALERT_SEV_WARNING ? "WARN" : "INFO",
             iface ? iface : "system", msg);
}

void alerts_init(const config_t *cfg)
{
    g_cfg = cfg;
    memset(g_alert_log, 0, sizeof(g_alert_log));
    memset(g_hyst_counters, 0, sizeof(g_hyst_counters));
    g_alert_count = 0;
    g_alert_head = 0;
    g_active_count = 0;
}

void alerts_check(const void *state_ptr)
{
    const net_state_t *state = (const net_state_t *)state_ptr;

    /* Count active alerts fresh each check */
    g_active_count = 0;

    for (int i = 0; i < state->num_ifaces; i++) {
        const net_iface_state_t *iface = &state->ifaces[i];

        /* Check interface down */
        if (!iface->info.is_up || !iface->info.is_running) {
            g_hyst_counters[i][ALERT_IFACE_DOWN]++;
            if (g_hyst_counters[i][ALERT_IFACE_DOWN] >= APP_ALERT_HYSTERESIS) {
                push_alert(ALERT_IFACE_DOWN, ALERT_SEV_CRITICAL,
                           iface->info.name, "Interface is down");
                g_hyst_counters[i][ALERT_IFACE_DOWN] = 0;
            }
        } else {
            g_hyst_counters[i][ALERT_IFACE_DOWN] = 0;
        }

        /* Check packet errors */
        if (iface->rates.rx_errors_per_sec + iface->rates.tx_errors_per_sec >
            APP_ALERT_ERR_THRESHOLD) {
            g_hyst_counters[i][ALERT_PACKET_ERRORS]++;
            if (g_hyst_counters[i][ALERT_PACKET_ERRORS] >= APP_ALERT_HYSTERESIS) {
                push_alert(ALERT_PACKET_ERRORS, ALERT_SEV_WARNING,
                           iface->info.name, "High packet error rate");
                g_hyst_counters[i][ALERT_PACKET_ERRORS] = 0;
            }
        } else {
            g_hyst_counters[i][ALERT_PACKET_ERRORS] = 0;
        }
    }

    /* Check gateway reachability */
    if (!state->connectivity.gateway_reachable) {
        g_hyst_counters[0][ALERT_GATEWAY_UNREACHABLE]++;
        if (g_hyst_counters[0][ALERT_GATEWAY_UNREACHABLE] >= APP_ALERT_HYSTERESIS) {
            push_alert(ALERT_GATEWAY_UNREACHABLE, ALERT_SEV_CRITICAL,
                       NULL, "Gateway unreachable");
            g_hyst_counters[0][ALERT_GATEWAY_UNREACHABLE] = 0;
        }
    } else {
        g_hyst_counters[0][ALERT_GATEWAY_UNREACHABLE] = 0;
    }

    /* Check high latency */
    if (state->connectivity.gateway_reachable &&
        state->connectivity.gateway_latency_ms > APP_ALERT_PING_CRIT_MS) {
        push_alert(ALERT_HIGH_LATENCY, ALERT_SEV_CRITICAL,
                   NULL, "Very high gateway latency");
    } else if (state->connectivity.gateway_reachable &&
               state->connectivity.gateway_latency_ms > APP_ALERT_PING_WARN_MS) {
        push_alert(ALERT_HIGH_LATENCY, ALERT_SEV_WARNING,
                   NULL, "High gateway latency");
    }

    /* Check DNS */
    if (!state->connectivity.dns_reachable) {
        g_hyst_counters[0][ALERT_DNS_FAILURE]++;
        if (g_hyst_counters[0][ALERT_DNS_FAILURE] >= APP_ALERT_HYSTERESIS) {
            push_alert(ALERT_DNS_FAILURE, ALERT_SEV_WARNING,
                       NULL, "DNS resolution failed");
            g_hyst_counters[0][ALERT_DNS_FAILURE] = 0;
        }
    } else {
        g_hyst_counters[0][ALERT_DNS_FAILURE] = 0;
    }

    /* Count all currently active alerts in the log */
    for (int i = 0; i < g_alert_count; i++) {
        if (g_alert_log[i].active) g_active_count++;
    }
}

int alerts_get_log(const alert_entry_t **out_entries, int *out_count)
{
    *out_entries = g_alert_log;
    *out_count = g_alert_count;
    return g_active_count;
}

int alerts_active_count(void)
{
    return g_active_count;
}

void alerts_clear_all(void)
{
    for (int i = 0; i < APP_MAX_ALERTS; i++) {
        g_alert_log[i].active = 0;
    }
    g_active_count = 0;
}
