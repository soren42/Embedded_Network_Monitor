/**
 * @file net_wifi.c
 * @brief WiFi statistics reader
 */

#include "net_wifi.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>

/*
 * Parse /proc/net/wireless for link quality, signal, and noise.
 *
 * Format (after two header lines):
 *   iface: status  link  level  noise  ...
 *
 * Values may have a trailing period (e.g. "70." means 70).
 */
static int parse_proc_wireless(const char *iface, int *link, int *signal_dbm,
                                int *noise_dbm)
{
    FILE *fp = fopen("/proc/net/wireless", "r");
    if (!fp)
        return -1;

    char line[256];
    int found = -1;

    /* Skip two header lines */
    if (!fgets(line, sizeof(line), fp) || !fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        const char *p = line;
        while (*p == ' ')
            p++;

        size_t nlen = strlen(iface);
        if (strncmp(p, iface, nlen) != 0 || p[nlen] != ':')
            continue;

        /* Found our interface -- skip past "name: status" */
        p += nlen + 1;

        int status;
        float f_link, f_signal, f_noise;
        if (sscanf(p, "%d %f %f %f", &status, &f_link, &f_signal, &f_noise) >= 4) {
            *link       = (int)f_link;
            *signal_dbm = (int)f_signal;
            *noise_dbm  = (int)f_noise;
            found = 0;
        }
        break;
    }

    fclose(fp);
    return found;
}

int net_wifi_read(const char *iface, net_wifi_info_t *info)
{
    if (!iface || !info)
        return -1;

    memset(info, 0, sizeof(*info));

    /* Parse /proc/net/wireless for quality/signal/noise */
    int link = 0, signal = 0, noise = 0;
    if (parse_proc_wireless(iface, &link, &signal, &noise) < 0)
        return -1;

    info->link_quality = link;
    info->signal_dbm   = signal;
    info->noise_dbm    = noise;

    /* Open a socket for wireless ioctls */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return -1;

    /* Get SSID via SIOCGIWESSID */
    struct iwreq wrq;
    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, iface, IFNAMSIZ - 1);

    char ssid_buf[IW_ESSID_MAX_SIZE + 1];
    memset(ssid_buf, 0, sizeof(ssid_buf));
    wrq.u.essid.pointer = ssid_buf;
    wrq.u.essid.length  = IW_ESSID_MAX_SIZE;

    if (ioctl(sock, SIOCGIWESSID, &wrq) == 0) {
        int len = wrq.u.essid.length;
        if (len > (int)(sizeof(info->ssid) - 1))
            len = (int)(sizeof(info->ssid) - 1);
        memcpy(info->ssid, ssid_buf, (size_t)len);
        info->ssid[len] = '\0';
    }

    /* Get bitrate via SIOCGIWRATE */
    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIWRATE, &wrq) == 0) {
        /* wrq.u.bitrate.value is in bps */
        info->bitrate_mbps = (double)wrq.u.bitrate.value / 1e6;
    }

    close(sock);
    return 0;
}
