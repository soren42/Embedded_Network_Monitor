/**
 * @file net_wifi_mgr.h
 * @brief WiFi network management via wpa_supplicant / iw
 */

#ifndef NET_WIFI_MGR_H
#define NET_WIFI_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MAX_NETWORKS 16
#define WIFI_SSID_MAX     64
#define WIFI_IFACE_MAX    16

typedef struct {
    char ssid[WIFI_SSID_MAX];
    int  signal_dbm;
    int  frequency_mhz;
    int  is_open;
} wifi_scan_result_t;

typedef struct {
    int                 enabled;
    int                 connected;
    char                iface[WIFI_IFACE_MAX];
    char                current_ssid[WIFI_SSID_MAX];
    int                 num_scan_results;
    wifi_scan_result_t  scan_results[WIFI_MAX_NETWORKS];
} wifi_mgr_state_t;

/**
 * Initialize WiFi manager, detect wireless interface.
 * @return 0 on success, -1 if no wireless interface found
 */
int wifi_mgr_init(void);

/**
 * Get the WiFi manager state.
 */
const wifi_mgr_state_t *wifi_mgr_get_state(void);

/**
 * Get the WiFi interface name (e.g. "wlan0").
 */
const char *wifi_mgr_get_iface(void);

/**
 * Enable or disable the WiFi interface.
 * @param enable  1 to bring up, 0 to bring down
 * @return 0 on success, -1 on error
 */
int wifi_mgr_set_enabled(int enable);

/**
 * Trigger a WiFi scan. Results available via wifi_mgr_get_state().
 * @return 0 on success, -1 on error
 */
int wifi_mgr_scan(void);

/**
 * Connect to a WiFi network.
 * @param ssid      Network SSID
 * @param password  Password (NULL or "" for open networks)
 * @return 0 on success, -1 on error
 */
int wifi_mgr_connect(const char *ssid, const char *password);

/**
 * Disconnect from current network.
 * @return 0 on success
 */
int wifi_mgr_disconnect(void);

/**
 * Update WiFi status (call periodically).
 */
void wifi_mgr_update(void);

#ifdef __cplusplus
}
#endif

#endif /* NET_WIFI_MGR_H */
