/**
 * @file net_interfaces.h
 * @brief Network interface enumeration via ioctl
 */

#ifndef NET_INTERFACES_H
#define NET_INTERFACES_H

#include "net_collector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enumerate active network interfaces (excluding loopback).
 *
 * Populates each entry with name, flags, IP, netmask, MAC, and
 * wireless status.
 *
 * @param list      Output array of interface info structs
 * @param max_count Capacity of @p list
 * @return Number of interfaces found, or -1 on error
 */
int net_enumerate_interfaces(net_iface_info_t *list, int max_count);

/**
 * Set or clear promiscuous mode on a network interface.
 *
 * @param iface_name  Interface name (e.g. "eth0")
 * @param enable      1 to enable, 0 to disable
 * @return 0 on success, -1 on error
 */
int net_set_promiscuous(const char *iface_name, int enable);

#ifdef __cplusplus
}
#endif

#endif /* NET_INTERFACES_H */
