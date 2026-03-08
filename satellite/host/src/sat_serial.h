/*
 * Serial port abstraction for satellite feeder
 */

#ifndef SAT_SERIAL_H
#define SAT_SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* Open serial port. Returns fd or -1 on error. */
int serial_open(const char *device, int baud_rate);

/* Write data to serial port. Returns bytes written or -1. */
int serial_write(int fd, const uint8_t *data, size_t len);

/* Close serial port. */
void serial_close(int fd);

#endif /* SAT_SERIAL_H */
