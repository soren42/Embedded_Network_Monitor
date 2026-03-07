/**
 * @file net_interfaces.c
 * @brief Network interface enumeration via ioctl
 */

#include "net_interfaces.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Check whether an interface name appears in /proc/net/wireless */
static int is_wireless_iface(const char *name)
{
    FILE *fp = fopen("/proc/net/wireless", "r");
    if (!fp)
        return 0;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        /* Each data line starts with "  ifname:" */
        const char *p = line;
        while (*p == ' ')
            p++;
        size_t nlen = strlen(name);
        if (strncmp(p, name, nlen) == 0 && p[nlen] == ':') {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

int net_enumerate_interfaces(net_iface_info_t *list, int max_count)
{
    if (!list || max_count <= 0)
        return -1;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return -1;

    /* Ask the kernel for the list of interfaces */
    struct ifconf ifc;
    char buf[2048];
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;

    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        close(sock);
        return -1;
    }

    int count = 0;
    int num_ifreqs = ifc.ifc_len / (int)sizeof(struct ifreq);

    for (int i = 0; i < num_ifreqs && count < max_count; i++) {
        struct ifreq *ifr = &ifc.ifc_req[i];

        /* Skip loopback */
        if (strcmp(ifr->ifr_name, "lo") == 0)
            continue;

        net_iface_info_t *info = &list[count];
        memset(info, 0, sizeof(*info));
        strncpy(info->name, ifr->ifr_name, APP_IFACE_NAME_MAX - 1);
        info->name[APP_IFACE_NAME_MAX - 1] = '\0';

        /* Flags (up / running) */
        struct ifreq req;
        memset(&req, 0, sizeof(req));
        strncpy(req.ifr_name, ifr->ifr_name, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFFLAGS, &req) == 0) {
            info->is_up      = (req.ifr_flags & IFF_UP)      ? 1 : 0;
            info->is_running  = (req.ifr_flags & IFF_RUNNING) ? 1 : 0;
        }

        /* IPv4 address */
        memset(&req, 0, sizeof(req));
        strncpy(req.ifr_name, ifr->ifr_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFADDR, &req) == 0) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&req.ifr_addr;
            info->ip_addr = sin->sin_addr.s_addr;
        }

        /* Netmask */
        memset(&req, 0, sizeof(req));
        strncpy(req.ifr_name, ifr->ifr_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFNETMASK, &req) == 0) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&req.ifr_netmask;
            info->netmask = sin->sin_addr.s_addr;
        }

        /* MAC address */
        memset(&req, 0, sizeof(req));
        strncpy(req.ifr_name, ifr->ifr_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFHWADDR, &req) == 0) {
            memcpy(info->mac, req.ifr_hwaddr.sa_data, 6);
        }

        /* Wireless check */
        info->is_wireless = is_wireless_iface(info->name);

        count++;
    }

    close(sock);
    return count;
}

int net_set_promiscuous(const char *iface_name, int enable)
{
    if (!iface_name) return -1;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.ifr_name, iface_name, IFNAMSIZ - 1);

    /* Read current flags */
    if (ioctl(sock, SIOCGIFFLAGS, &req) < 0) {
        close(sock);
        return -1;
    }

    if (enable)
        req.ifr_flags |= IFF_PROMISC;
    else
        req.ifr_flags &= ~IFF_PROMISC;

    int ret = ioctl(sock, SIOCSIFFLAGS, &req);
    close(sock);
    return (ret < 0) ? -1 : 0;
}
