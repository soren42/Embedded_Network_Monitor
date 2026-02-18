/**
 * @file net_connections.h
 * @brief TCP/UDP connection table parser from /proc/net
 */

#ifndef NET_CONNECTIONS_H
#define NET_CONNECTIONS_H

#include "net_collector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Collect active TCP and UDP connections from /proc/net/tcp and /proc/net/udp.
 *
 * @param conns      Output array of connection entries
 * @param max_count  Capacity of @p conns
 * @return Total number of connections found, or -1 on error
 */
int net_collect_connections(net_connection_t *conns, int max_count);

#ifdef __cplusplus
}
#endif

#endif /* NET_CONNECTIONS_H */
