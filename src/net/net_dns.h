/**
 * @file net_dns.h
 * @brief Simple DNS query probe over UDP
 */

#ifndef NET_DNS_H
#define NET_DNS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send a minimal DNS A-record query and wait for any valid response.
 *
 * @param dns_server  Dotted-decimal IPv4 address of the DNS server
 * @param hostname    Domain name to query (e.g. "google.com")
 * @param timeout_ms  Maximum time to wait for reply (milliseconds)
 * @param latency_ms  If non-NULL, receives round-trip time on success
 * @return 1 if a DNS response was received, 0 on timeout or error
 */
int net_dns_probe(const char *dns_server, const char *hostname,
                  int timeout_ms, double *latency_ms);

#ifdef __cplusplus
}
#endif

#endif /* NET_DNS_H */
