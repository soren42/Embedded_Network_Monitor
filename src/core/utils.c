#include "utils.h"
#include <stdio.h>
#include <time.h>

void format_bytes(uint64_t bytes, char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return;

    if (bytes >= (uint64_t)1024 * 1024 * 1024) {
        snprintf(buf, (size_t)buf_len, "%.2f GB",
                 (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= (uint64_t)1024 * 1024) {
        snprintf(buf, (size_t)buf_len, "%.2f MB",
                 (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buf, (size_t)buf_len, "%.2f KB",
                 (double)bytes / 1024.0);
    } else {
        snprintf(buf, (size_t)buf_len, "%llu B",
                 (unsigned long long)bytes);
    }
}

void format_rate(double bytes_per_sec, char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return;

    if (bytes_per_sec >= 1024.0 * 1024.0 * 1024.0) {
        snprintf(buf, (size_t)buf_len, "%.2f GB/s",
                 bytes_per_sec / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes_per_sec >= 1024.0 * 1024.0) {
        snprintf(buf, (size_t)buf_len, "%.2f MB/s",
                 bytes_per_sec / (1024.0 * 1024.0));
    } else if (bytes_per_sec >= 1024.0) {
        snprintf(buf, (size_t)buf_len, "%.2f KB/s",
                 bytes_per_sec / 1024.0);
    } else {
        snprintf(buf, (size_t)buf_len, "%.0f B/s", bytes_per_sec);
    }
}

void format_duration(uint32_t seconds, char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return;

    uint32_t days  = seconds / 86400;
    uint32_t hours = (seconds % 86400) / 3600;
    uint32_t mins  = (seconds % 3600) / 60;
    uint32_t secs  = seconds % 60;

    if (days > 0) {
        snprintf(buf, (size_t)buf_len, "%ud %uh", days, hours);
    } else if (hours > 0) {
        snprintf(buf, (size_t)buf_len, "%uh %um", hours, mins);
    } else if (mins > 0) {
        snprintf(buf, (size_t)buf_len, "%um %us", mins, secs);
    } else {
        snprintf(buf, (size_t)buf_len, "%us", secs);
    }
}

void format_ip(uint32_t ip, char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return;

    snprintf(buf, (size_t)buf_len, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >>  8) & 0xFF,
              ip        & 0xFF);
}

void format_mac(const uint8_t mac[6], char *buf, int buf_len)
{
    if (!buf || buf_len <= 0 || !mac) return;

    snprintf(buf, (size_t)buf_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void format_timestamp(uint32_t epoch, char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return;

    time_t t = (time_t)epoch;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    snprintf(buf, (size_t)buf_len, "%02d:%02d:%02d",
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}
