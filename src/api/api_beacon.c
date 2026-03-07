/**
 * @file api_beacon.c
 * @brief UDP discovery beacon broadcast
 */

#include "api_beacon.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/time.h>

#define BEACON_PORT         5300
#define BEACON_INTERVAL_MS  5000

static int s_sock = -1;
static int s_api_port;
static uint64_t s_last_beacon_ms;

static uint64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int api_beacon_init(config_t *cfg)
{
    s_api_port = config_get_int(cfg, "api_port", 8080);

    s_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s_sock < 0) {
        LOG_WARN("Beacon: cannot create socket");
        return -1;
    }

    int broadcast = 1;
    setsockopt(s_sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    s_last_beacon_ms = 0;
    LOG_INFO("Beacon: initialized (port %d, interval %dms)",
             BEACON_PORT, BEACON_INTERVAL_MS);
    return 0;
}

static void send_beacon(void)
{
    if (s_sock < 0) return;

    char hostname[64] = "";
    gethostname(hostname, sizeof(hostname));

    /* Find first non-loopback IPv4 address */
    char ip_str[INET_ADDRSTRLEN] = "0.0.0.0";
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                continue;
            if (ifa->ifa_flags & IFF_LOOPBACK)
                continue;
            if (!(ifa->ifa_flags & IFF_UP))
                continue;
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip_str, sizeof(ip_str));
            break;
        }
        freeifaddrs(ifap);
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"service\":\"netmon\",\"version\":\"3.0.0\","
             "\"port\":%d,\"hostname\":\"%s\",\"ip\":\"%s\"}",
             s_api_port, hostname, ip_str);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(BEACON_PORT);
    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    sendto(s_sock, payload, strlen(payload), 0,
           (struct sockaddr *)&dest, sizeof(dest));
}

void api_beacon_tick(void)
{
    if (s_sock < 0) return;

    uint64_t now = now_ms();
    if (now - s_last_beacon_ms >= BEACON_INTERVAL_MS) {
        send_beacon();
        s_last_beacon_ms = now;
    }
}

void api_beacon_shutdown(void)
{
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}
