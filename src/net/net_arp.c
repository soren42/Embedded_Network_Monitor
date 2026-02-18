/**
 * @file net_arp.c
 * @brief ARP table reader from /proc/net/arp
 */

#include "net_arp.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#define ARP_HW_ETHER    0x1
#define ARP_FLAGS_COMP  0x2

int net_read_arp(net_arp_entry_t *entries, int max_count)
{
    if (!entries || max_count <= 0)
        return -1;

    FILE *fp = fopen("/proc/net/arp", "r");
    if (!fp)
        return -1;

    char line[256];
    int count = 0;

    /* Skip header line */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        char ip_str[32], mac_str[32], mask_str[32], dev[32];
        int hw_type, flags;

        /* Format: IP HW_type Flags MAC Mask Device */
        int n = sscanf(line, "%31s 0x%x 0x%x %31s %31s %31s",
                       ip_str, &hw_type, &flags, mac_str, mask_str, dev);
        if (n < 6)
            continue;

        /* Only include complete Ethernet entries */
        if (hw_type != ARP_HW_ETHER || flags != ARP_FLAGS_COMP)
            continue;

        net_arp_entry_t *e = &entries[count];
        memset(e, 0, sizeof(*e));

        /* Parse IP address */
        struct in_addr in;
        if (inet_pton(AF_INET, ip_str, &in) != 1)
            continue;
        e->ip_addr = in.s_addr;

        /* Parse MAC address (aa:bb:cc:dd:ee:ff) */
        unsigned int m[6];
        if (sscanf(mac_str, "%x:%x:%x:%x:%x:%x",
                   &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
            continue;
        for (int i = 0; i < 6; i++)
            e->mac[i] = (uint8_t)m[i];

        /* Device name */
        strncpy(e->device, dev, APP_IFACE_NAME_MAX - 1);
        e->device[APP_IFACE_NAME_MAX - 1] = '\0';

        count++;
    }

    fclose(fp);
    return count;
}
