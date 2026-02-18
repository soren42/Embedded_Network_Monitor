#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

void format_bytes(uint64_t bytes, char *buf, int buf_len);
void format_rate(double bytes_per_sec, char *buf, int buf_len);
void format_duration(uint32_t seconds, char *buf, int buf_len);
void format_ip(uint32_t ip, char *buf, int buf_len);
void format_mac(const uint8_t mac[6], char *buf, int buf_len);
void format_timestamp(uint32_t epoch, char *buf, int buf_len);

#endif /* UTILS_H */
