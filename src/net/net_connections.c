/**
 * @file net_connections.c
 * @brief TCP/UDP connection table parser from /proc/net
 */

#include "net_connections.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/*
 * Parse a single /proc/net/tcp or /proc/net/udp file.
 *
 * Each data line (after header) has the format:
 *   sl  local_address  remote_address  st  ...
 * where addresses are hex IP:port (e.g. 0100007F:0050).
 *
 * Returns number of entries written, or -1 on error.
 */
static int parse_proc_net(const char *path, uint8_t protocol,
                          net_connection_t *conns, int max_count)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char line[512];
    int count = 0;

    /* Skip header line */
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) && count < max_count) {
        unsigned int sl;
        unsigned int local_ip, local_port;
        unsigned int remote_ip, remote_port;
        unsigned int state;

        /*
         * Fields:  sl  local_address  rem_address  st  ...
         * Example: 0: 0100007F:0035 00000000:0000 0A ...
         */
        int n = sscanf(line,
                       " %u: %X:%X %X:%X %X",
                       &sl,
                       &local_ip, &local_port,
                       &remote_ip, &remote_port,
                       &state);
        if (n < 6)
            continue;

        net_connection_t *c = &conns[count];
        c->local_addr  = (uint32_t)local_ip;
        c->local_port  = (uint16_t)local_port;
        c->remote_addr = (uint32_t)remote_ip;
        c->remote_port = (uint16_t)remote_port;
        c->state       = (uint8_t)state;
        c->protocol    = protocol;

        count++;
    }

    fclose(fp);
    return count;
}

int net_collect_connections(net_connection_t *conns, int max_count)
{
    if (!conns || max_count <= 0)
        return -1;

    int total = 0;

    /* Parse TCP connections (protocol 6) */
    int tcp = parse_proc_net("/proc/net/tcp", 6, conns, max_count);
    if (tcp > 0)
        total += tcp;

    /* Parse UDP connections (protocol 17) into remaining space */
    int remaining = max_count - total;
    if (remaining > 0) {
        int udp = parse_proc_net("/proc/net/udp", 17, conns + total, remaining);
        if (udp > 0)
            total += udp;
    }

    return total;
}
