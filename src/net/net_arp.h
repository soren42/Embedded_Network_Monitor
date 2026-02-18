/**
 * @file net_arp.h
 * @brief ARP table reader from /proc/net/arp
 */

#ifndef NET_ARP_H
#define NET_ARP_H

#include "net_collector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the kernel ARP table from /proc/net/arp.
 *
 * Only complete Ethernet entries (HW type 0x1, flags 0x2) are included.
 *
 * @param entries    Output array of ARP entries
 * @param max_count  Capacity of @p entries
 * @return Number of entries read, or -1 on error
 */
int net_read_arp(net_arp_entry_t *entries, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* NET_ARP_H */
