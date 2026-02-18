/**
 * @file net_dns.c
 * @brief Simple DNS A-record query probe over UDP
 */

#include "net_dns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/* DNS header (12 bytes) */
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

#define DNS_PORT          53
#define DNS_MAX_PKT       512
#define DNS_FLAG_RD       0x0100   /* Recursion Desired */
#define DNS_TYPE_A        1
#define DNS_CLASS_IN      1

/*
 * Encode a dotted hostname into DNS QNAME wire format.
 * Returns number of bytes written, or -1 on error.
 */
static int encode_qname(uint8_t *buf, int buflen, const char *hostname)
{
    int pos = 0;
    const char *p = hostname;

    while (*p) {
        const char *dot = strchr(p, '.');
        int label_len = dot ? (int)(dot - p) : (int)strlen(p);

        if (label_len == 0 || label_len > 63)
            return -1;
        if (pos + 1 + label_len >= buflen)
            return -1;

        buf[pos++] = (uint8_t)label_len;
        memcpy(&buf[pos], p, (size_t)label_len);
        pos += label_len;

        p += label_len;
        if (*p == '.')
            p++;
    }

    if (pos + 1 >= buflen)
        return -1;
    buf[pos++] = 0;  /* root label */

    return pos;
}

static double timespec_diff_ms(const struct timespec *a, const struct timespec *b)
{
    double sec  = (double)(b->tv_sec  - a->tv_sec);
    double nsec = (double)(b->tv_nsec - a->tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

int net_dns_probe(const char *dns_server, const char *hostname,
                  int timeout_ms, double *latency_ms)
{
    if (!dns_server || !hostname)
        return 0;

    /* Resolve server address */
    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port   = htons(DNS_PORT);
    if (inet_pton(AF_INET, dns_server, &srv.sin_addr) != 1)
        return 0;

    /* Build query packet */
    uint8_t pkt[DNS_MAX_PKT];
    memset(pkt, 0, sizeof(pkt));

    dns_header_t *hdr = (dns_header_t *)pkt;
    uint16_t txn_id   = (uint16_t)(getpid() ^ time(NULL));
    hdr->id      = htons(txn_id);
    hdr->flags   = htons(DNS_FLAG_RD);
    hdr->qdcount = htons(1);

    int offset = (int)sizeof(dns_header_t);

    /* Encode the question */
    int qname_len = encode_qname(pkt + offset, DNS_MAX_PKT - offset - 4, hostname);
    if (qname_len < 0)
        return 0;
    offset += qname_len;

    /* QTYPE = A, QCLASS = IN */
    pkt[offset++] = 0;
    pkt[offset++] = (uint8_t)DNS_TYPE_A;
    pkt[offset++] = 0;
    pkt[offset++] = (uint8_t)DNS_CLASS_IN;

    /* Create UDP socket */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return 0;

    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct timespec t_send, t_recv;
    clock_gettime(CLOCK_MONOTONIC, &t_send);

    ssize_t sent = sendto(sock, pkt, (size_t)offset, 0,
                          (struct sockaddr *)&srv, sizeof(srv));
    if (sent < 0) {
        close(sock);
        return 0;
    }

    /* Wait for reply */
    uint8_t resp[DNS_MAX_PKT];
    ssize_t n = recvfrom(sock, resp, sizeof(resp), 0, NULL, NULL);
    close(sock);

    if (n < (ssize_t)sizeof(dns_header_t))
        return 0;

    /* Verify transaction ID matches */
    dns_header_t *rhdr = (dns_header_t *)resp;
    if (ntohs(rhdr->id) != txn_id)
        return 0;

    clock_gettime(CLOCK_MONOTONIC, &t_recv);
    if (latency_ms)
        *latency_ms = timespec_diff_ms(&t_send, &t_recv);

    return 1;
}
