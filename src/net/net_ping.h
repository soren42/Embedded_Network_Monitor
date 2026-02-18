/**
 * @file net_ping.h
 * @brief Simple ICMP echo (ping) probe
 */

#ifndef NET_PING_H
#define NET_PING_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send a single ICMP echo request and wait for a reply.
 *
 * @param target      Dotted-decimal IPv4 address to ping
 * @param timeout_ms  Maximum time to wait for reply (milliseconds)
 * @param latency_ms  If non-NULL, receives round-trip time on success
 * @return 1 if target replied, 0 on timeout or error
 */
int net_ping(const char *target, int timeout_ms, double *latency_ms);

#ifdef __cplusplus
}
#endif

#endif /* NET_PING_H */
