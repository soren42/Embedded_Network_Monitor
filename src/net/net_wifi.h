/**
 * @file net_wifi.h
 * @brief WiFi statistics reader
 */

#ifndef NET_WIFI_H
#define NET_WIFI_H

#include "net_collector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read WiFi statistics for a given interface.
 *
 * Parses /proc/net/wireless for link/signal/noise and uses
 * wireless ioctls for SSID and bitrate.
 *
 * @param iface  Interface name (e.g. "wlan0")
 * @param info   Output WiFi info struct
 * @return 0 on success, -1 if the interface is not wireless or on error
 */
int net_wifi_read(const char *iface, net_wifi_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* NET_WIFI_H */
