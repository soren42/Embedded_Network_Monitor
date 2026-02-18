/**
 * @file net_ping.c
 * @brief Simple ICMP echo (ping) probe
 */

#include "net_ping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

/* ICMP echo payload size (bytes) */
#define PING_DATA_LEN  32
#define PING_PKT_LEN   (sizeof(struct icmp) + PING_DATA_LEN)

/* Sequence counter persisted across calls for unique identification */
static uint16_t g_seq = 0;

static uint16_t icmp_checksum(const void *data, int len)
{
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1)
        sum += *(const uint8_t *)p;

    sum  = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (uint16_t)~sum;
}

static double timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    double sec  = (double)(b->tv_sec  - a->tv_sec);
    double nsec = (double)(b->tv_nsec - a->tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

int net_ping(const char *target, int timeout_ms, double *latency_ms)
{
    if (!target)
        return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, target, &addr.sin_addr) != 1)
        return 0;

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0)
        return 0;

    /* Set receive timeout */
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Build ICMP echo request */
    uint8_t pkt[PING_PKT_LEN];
    memset(pkt, 0, sizeof(pkt));

    struct icmp *icmp_hdr = (struct icmp *)pkt;
    icmp_hdr->icmp_type  = ICMP_ECHO;
    icmp_hdr->icmp_code  = 0;
    icmp_hdr->icmp_id    = htons((uint16_t)getpid());
    icmp_hdr->icmp_seq   = htons(++g_seq);
    icmp_hdr->icmp_cksum = 0;

    /* Fill payload with a simple pattern */
    for (int i = 0; i < PING_DATA_LEN; i++)
        pkt[sizeof(struct icmp) + i] = (uint8_t)(i & 0xFF);

    icmp_hdr->icmp_cksum = icmp_checksum(pkt, sizeof(pkt));

    /* Send and time the round trip */
    struct timespec t_send, t_recv;
    clock_gettime(CLOCK_MONOTONIC, &t_send);

    ssize_t sent = sendto(sock, pkt, sizeof(pkt), 0,
                          (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        close(sock);
        return 0;
    }

    /* Wait for reply */
    uint8_t recv_buf[256];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    for (;;) {
        ssize_t n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0,
                             (struct sockaddr *)&from, &fromlen);
        if (n <= 0) {
            close(sock);
            return 0;   /* timeout or error */
        }

        /* Skip IP header */
        struct ip *ip_hdr = (struct ip *)recv_buf;
        int ip_hlen = ip_hdr->ip_hl * 4;
        if (n < ip_hlen + (ssize_t)sizeof(struct icmp))
            continue;

        struct icmp *reply = (struct icmp *)(recv_buf + ip_hlen);
        if (reply->icmp_type == ICMP_ECHOREPLY &&
            reply->icmp_id   == htons((uint16_t)getpid()) &&
            reply->icmp_seq  == htons(g_seq))
        {
            clock_gettime(CLOCK_MONOTONIC, &t_recv);
            if (latency_ms)
                *latency_ms = timespec_diff_ms(&t_send, &t_recv);
            close(sock);
            return 1;
        }

        /* Not our reply -- check if we've exceeded the timeout */
        clock_gettime(CLOCK_MONOTONIC, &t_recv);
        if (timespec_diff_ms(&t_send, &t_recv) >= (double)timeout_ms) {
            close(sock);
            return 0;
        }
    }
}
