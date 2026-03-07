/**
 * @file net_discovery.c
 * @brief WAN link discovery - next hop detection and link quality
 *
 * Uses raw ICMP with increasing TTL to trace the path beyond the gateway,
 * then measures jitter and packet loss to the first upstream hop.
 */

#include "net_discovery.h"
#include "net_ping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

/**
 * Send an ICMP echo with a specific TTL and capture the responding IP.
 * Returns 0 if we got a TIME_EXCEEDED or ECHO_REPLY, -1 on timeout/error.
 */
static int probe_ttl(const char *target_ip, int ttl, int timeout_ms,
                     uint32_t *responder_ip, double *latency_ms)
{
    *responder_ip = 0;
    *latency_ms = 0.0;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) return -1;

    /* Set TTL */
    if (setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        close(sock);
        return -1;
    }

    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, target_ip, &dest.sin_addr);

    /* Build ICMP echo request */
    struct {
        struct icmphdr hdr;
        char data[8];
    } pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.type = ICMP_ECHO;
    pkt.hdr.code = 0;
    pkt.hdr.un.echo.id = (uint16_t)getpid();
    pkt.hdr.un.echo.sequence = (uint16_t)ttl;

    /* Compute checksum */
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)&pkt;
    for (int i = 0; i < (int)(sizeof(pkt) / 2); i++)
        sum += ptr[i];
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    pkt.hdr.checksum = (uint16_t)~sum;

    struct timeval start, end;
    gettimeofday(&start, NULL);

    if (sendto(sock, &pkt, sizeof(pkt), 0,
               (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        close(sock);
        return -1;
    }

    /* Receive response */
    char recv_buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    int n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                     (struct sockaddr *)&from, &fromlen);

    gettimeofday(&end, NULL);
    close(sock);

    if (n < 0) return -1;

    *latency_ms = (end.tv_sec - start.tv_sec) * 1000.0 +
                  (end.tv_usec - start.tv_usec) / 1000.0;
    *responder_ip = from.sin_addr.s_addr;

    /* Check ICMP type */
    struct iphdr *ip_hdr = (struct iphdr *)recv_buf;
    int ip_hlen = ip_hdr->ihl * 4;
    if (n < ip_hlen + (int)sizeof(struct icmphdr))
        return -1;

    struct icmphdr *icmp = (struct icmphdr *)(recv_buf + ip_hlen);
    if (icmp->type == ICMP_TIME_EXCEEDED || icmp->type == ICMP_ECHOREPLY)
        return 0;

    return -1;
}

int discovery_trace_wan(const char *gateway_ip, wan_discovery_t *result)
{
    if (!gateway_ip || !result) return -1;

    memset(result, 0, sizeof(*result));

    /* Use a well-known public IP as the trace target (Cloudflare DNS) */
    const char *trace_target = "1.1.1.1";

    for (int ttl = 1; ttl <= DISCOVERY_MAX_HOPS; ttl++) {
        uint32_t resp_ip = 0;
        double lat = 0.0;

        int ok = probe_ttl(trace_target, ttl, 2000, &resp_ip, &lat);
        if (ok < 0) {
            /* Timeout at this TTL - record as unreachable hop */
            if (result->num_hops < DISCOVERY_MAX_HOPS) {
                discovery_hop_t *hop = &result->hops[result->num_hops];
                hop->ip_addr = 0;
                hop->latency_ms = 0;
                hop->reachable = 0;
                snprintf(hop->ip_str, sizeof(hop->ip_str), "*");
                result->num_hops++;
            }
            continue;
        }

        if (result->num_hops < DISCOVERY_MAX_HOPS) {
            discovery_hop_t *hop = &result->hops[result->num_hops];
            hop->ip_addr = resp_ip;
            hop->latency_ms = lat;
            hop->reachable = 1;

            struct in_addr addr;
            addr.s_addr = resp_ip;
            snprintf(hop->ip_str, sizeof(hop->ip_str), "%s",
                     inet_ntoa(addr));
            result->num_hops++;
        }

        /* Check if this hop is beyond the gateway */
        struct in_addr gw_addr;
        inet_pton(AF_INET, gateway_ip, &gw_addr);
        if (resp_ip != gw_addr.s_addr && result->num_hops >= 2) {
            result->wan_reachable = 1;
            result->wan_latency_ms = lat;
        }

        /* If we got an echo reply, we reached the destination */
        if (resp_ip == inet_addr(trace_target))
            break;
    }

    return 0;
}

int discovery_wan_quality(const char *target_ip, wan_discovery_t *result)
{
    if (!target_ip || !result) return -1;

    /* Send 5 pings and compute jitter + loss */
    double latencies[5];
    int successes = 0;
    int attempts = 5;

    for (int i = 0; i < attempts; i++) {
        double lat = 0.0;
        if (net_ping(target_ip, 1000, &lat)) {
            latencies[successes++] = lat;
        }
    }

    if (successes == 0) {
        result->wan_packet_loss_pct = 100;
        result->wan_jitter_ms = 0;
        return 0;
    }

    result->wan_packet_loss_pct = ((attempts - successes) * 100) / attempts;

    /* Compute average and jitter (mean deviation) */
    double avg = 0;
    for (int i = 0; i < successes; i++) avg += latencies[i];
    avg /= successes;
    result->wan_latency_ms = avg;

    double jitter = 0;
    for (int i = 0; i < successes; i++) {
        double d = latencies[i] - avg;
        jitter += (d < 0) ? -d : d;
    }
    result->wan_jitter_ms = (successes > 1) ? jitter / successes : 0;

    return 0;
}
