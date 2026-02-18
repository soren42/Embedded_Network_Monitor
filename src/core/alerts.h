/**
 * @file alerts.h
 * @brief Alert threshold engine with hysteresis
 */

#ifndef ALERTS_H
#define ALERTS_H

#include <stdint.h>
#include "app_conf.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ALERT_IFACE_DOWN = 0,
    ALERT_HIGH_BANDWIDTH,
    ALERT_PACKET_ERRORS,
    ALERT_PACKET_DROPS,
    ALERT_GATEWAY_UNREACHABLE,
    ALERT_DNS_FAILURE,
    ALERT_HIGH_LATENCY,
    ALERT_TYPE_COUNT
} alert_type_t;

typedef enum {
    ALERT_SEV_INFO = 0,
    ALERT_SEV_WARNING,
    ALERT_SEV_CRITICAL
} alert_severity_t;

typedef struct {
    alert_type_t     type;
    alert_severity_t severity;
    uint32_t         timestamp;
    char             iface[APP_IFACE_NAME_MAX];
    char             message[128];
    int              active;
} alert_entry_t;

/**
 * Initialize the alert system.
 */
void alerts_init(const config_t *cfg);

/**
 * Check all thresholds against current network state.
 * Called periodically from the medium timer.
 */
void alerts_check(const void *state);

/**
 * Get the alert log ring buffer.
 * @param out_entries  Pointer to receive array pointer
 * @param out_count    Pointer to receive count of valid entries
 * @return Number of currently active alerts
 */
int alerts_get_log(const alert_entry_t **out_entries, int *out_count);

/**
 * Get the count of currently active alerts.
 */
int alerts_active_count(void);

/**
 * Clear all active alerts.
 */
void alerts_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ALERTS_H */
